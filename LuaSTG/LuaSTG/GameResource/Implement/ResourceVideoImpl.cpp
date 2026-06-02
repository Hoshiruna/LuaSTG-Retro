#include "GameResource/Implement/ResourceVideoImpl.hpp"
#include "AppFrame.h"

#include <algorithm>
#include <cmath>
#include <objbase.h>
#include <stdexcept>
#include <utility>

namespace {
	double normalize_time(double value, double duration) {
		value = value < 0.0 ? 0.0 : value;
		if (duration > 0.0) {
			value = value < duration ? value : duration;
		}
		return value;
	}
}

namespace luastg {
	ResourceVideoImpl::ResourceVideoImpl(char const* const name, char const* const path, bool const loop)
		: ResourceBaseImpl(ResourceType::Texture, name)
		, m_loop(loop) {
		if (!core::IVideoDecoder::create(path, m_decoder.put())) {
			throw std::runtime_error("create video decoder failed");
		}
		initialize();
	}

	ResourceVideoImpl::ResourceVideoImpl(char const* const name, core::IVideoDecoder* const decoder, bool const loop)
		: ResourceBaseImpl(ResourceType::Texture, name)
		, m_decoder(decoder)
		, m_loop(loop) {
		if (!m_decoder) {
			throw std::runtime_error("create video decoder failed");
		}
		initialize();
	}

	void ResourceVideoImpl::initialize() {
		auto const width = m_decoder->getWidth();
		auto const height = m_decoder->getHeight();
		if (width == 0 || height == 0) {
			throw std::runtime_error("invalid video size");
		}
		m_duration = m_decoder->getDuration();
		if (!LAPP.GetAppModel()->getDevice()->createTexture(core::Vector2U(width, height), m_texture.put())) {
			throw std::runtime_error("create video texture failed");
		}
		if (!uploadFrameAtCurrentTime()) {
			throw std::runtime_error("decode first video frame failed");
		}
		(void)m_decoder->seek(0.0);
		m_time = 0.0;
		m_worker = std::thread(&ResourceVideoImpl::workerMain, this);
	}

	ResourceVideoImpl::~ResourceVideoImpl() {
		{
			std::lock_guard const lock(m_worker_mutex);
			m_worker_exit = true;
		}
		m_worker_cv.notify_one();
		if (m_worker.joinable()) {
			m_worker.join();
		}
	}

	bool ResourceVideoImpl::uploadFrame(core::VideoFrame const& frame) {
		if (!frame.pixels || frame.width == 0 || frame.height == 0) {
			return false;
		}
		auto const texture_size = m_texture->getSize();
		if (texture_size.x != frame.width || texture_size.y != frame.height) {
			return false;
		}
		return m_texture->uploadPixelData(
			core::RectU(0, 0, frame.width, frame.height),
			frame.pixels->data(),
			frame.pitch);
	}

	bool ResourceVideoImpl::uploadFrameAtCurrentTime() {
		if (!m_decoder->seek(m_time)) {
			return false;
		}
		core::VideoFrame frame;
		switch (m_decoder->readFrame(frame)) {
		case core::VideoDecodeResult::Frame:
			return uploadFrame(frame);
		case core::VideoDecodeResult::End:
			return true;
		case core::VideoDecodeResult::Error:
		default:
			return false;
		}
	}

	void ResourceVideoImpl::requestWorkerSeek(double const seconds, bool const decode_one_frame) {
		{
			std::lock_guard const lock(m_worker_mutex);
			m_frame_queue.clear();
			m_worker_seek_time = seconds;
			m_worker_seek_requested = true;
			m_worker_seek_decode_one_frame = decode_one_frame;
			m_worker_eof = false;
			m_worker_error = false;
			m_force_upload_next_frame = decode_one_frame;
			m_worker_serial += 1;
		}
		m_worker_cv.notify_one();
	}

	void ResourceVideoImpl::workerMain() {
		HRESULT const hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		bool const should_uninitialize_com = SUCCEEDED(hr);

		for (;;) {
			bool do_seek = false;
			bool seek_decode_one_frame = false;
			uint64_t task_serial = 0;
			double seek_time = 0.0;

			{
				std::unique_lock lock(m_worker_mutex);
				m_worker_cv.wait(lock, [this] {
					return m_worker_exit
						|| m_worker_seek_requested
						|| (m_worker_decode_enabled && !m_worker_eof && !m_worker_error && m_frame_queue.size() < max_queued_frames);
				});

				if (m_worker_exit) {
					break;
				}

				if (m_worker_seek_requested) {
					do_seek = true;
					seek_time = m_worker_seek_time;
					seek_decode_one_frame = m_worker_seek_decode_one_frame;
					task_serial = m_worker_serial;
					m_worker_seek_requested = false;
					m_worker_seek_decode_one_frame = false;
					m_frame_queue.clear();
					m_worker_eof = false;
					m_worker_error = false;
				}
				else {
					task_serial = m_worker_serial;
				}
			}

			if (do_seek) {
				bool ok = m_decoder->seek(seek_time);
				if (ok && seek_decode_one_frame) {
					core::VideoFrame frame;
					switch (m_decoder->readFrame(frame)) {
					case core::VideoDecodeResult::Frame:
						{
							std::lock_guard const lock(m_worker_mutex);
							if (task_serial == m_worker_serial) {
								m_frame_queue.push_back(std::move(frame));
							}
						}
						break;
					case core::VideoDecodeResult::End:
						{
							std::lock_guard const lock(m_worker_mutex);
							if (task_serial == m_worker_serial) {
								m_worker_eof = true;
							}
						}
						break;
					case core::VideoDecodeResult::Error:
					default:
						ok = false;
						break;
					}
				}
				if (!ok) {
					std::lock_guard const lock(m_worker_mutex);
					if (task_serial == m_worker_serial) {
						m_worker_error = true;
					}
				}
				continue;
			}

			core::VideoFrame frame;
			switch (m_decoder->readFrame(frame)) {
			case core::VideoDecodeResult::Frame:
				{
					std::lock_guard const lock(m_worker_mutex);
					if (task_serial == m_worker_serial && !m_worker_seek_requested && m_frame_queue.size() < max_queued_frames) {
						m_frame_queue.push_back(std::move(frame));
					}
				}
				break;
			case core::VideoDecodeResult::End:
				{
					std::lock_guard const lock(m_worker_mutex);
					if (task_serial == m_worker_serial) {
						m_worker_eof = true;
					}
				}
				break;
			case core::VideoDecodeResult::Error:
			default:
				{
					std::lock_guard const lock(m_worker_mutex);
					if (task_serial == m_worker_serial) {
						m_worker_error = true;
					}
				}
				break;
			}
		}

		if (should_uninitialize_com) {
			CoUninitialize();
		}
	}

	void ResourceVideoImpl::Update(double const delta_seconds) {
		core::VideoFrame frame_to_upload;
		bool has_frame_to_upload = false;
		bool should_request_loop_seek = false;
		double loop_seek_time = 0.0;

		if (m_state != VideoPlaybackState::Playing) {
			{
				std::lock_guard const lock(m_worker_mutex);
				if (m_force_upload_next_frame && !m_frame_queue.empty()) {
					frame_to_upload = std::move(m_frame_queue.front());
					m_frame_queue.pop_front();
					m_force_upload_next_frame = false;
					has_frame_to_upload = true;
				}
			}
			if (has_frame_to_upload) {
				(void)uploadFrame(frame_to_upload);
			}
			return;
		}

		m_time += delta_seconds < 0.0 ? 0.0 : delta_seconds;
		if (m_duration > 0.0 && m_time >= m_duration) {
			if (m_loop) {
				m_time = std::fmod(m_time, m_duration);
				should_request_loop_seek = true;
				loop_seek_time = m_time;
			}
			else {
				m_time = m_duration;
				m_state = VideoPlaybackState::Ended;
			}
		}

		{
			std::lock_guard const lock(m_worker_mutex);
			if (m_worker_error) {
				m_state = VideoPlaybackState::Ended;
			}
			if (!should_request_loop_seek) {
				while (!m_frame_queue.empty()) {
					if (m_force_upload_next_frame || m_frame_queue.front().timestamp <= m_time) {
						frame_to_upload = std::move(m_frame_queue.front());
						m_frame_queue.pop_front();
						m_force_upload_next_frame = false;
						has_frame_to_upload = true;
						continue;
					}
					break;
				}
				if (m_worker_eof && m_frame_queue.empty() && m_duration <= 0.0) {
					m_state = VideoPlaybackState::Ended;
				}
			}
		}

		if (has_frame_to_upload) {
			(void)uploadFrame(frame_to_upload);
		}
		if (m_state == VideoPlaybackState::Ended) {
			std::lock_guard const lock(m_worker_mutex);
			m_worker_decode_enabled = false;
		}
		if (should_request_loop_seek) {
			requestWorkerSeek(loop_seek_time, true);
		}
		else {
			m_worker_cv.notify_one();
		}
	}

	bool ResourceVideoImpl::Play(bool restart) {
		if (restart || m_state == VideoPlaybackState::Ended) {
			m_time = 0.0;
			requestWorkerSeek(0.0, true);
		}
		m_state = VideoPlaybackState::Playing;
		{
			std::lock_guard const lock(m_worker_mutex);
			m_worker_decode_enabled = true;
			m_worker_error = false;
		}
		m_worker_cv.notify_one();
		return true;
	}

	bool ResourceVideoImpl::Pause() {
		if (m_state == VideoPlaybackState::Playing) {
			m_state = VideoPlaybackState::Paused;
		}
		{
			std::lock_guard const lock(m_worker_mutex);
			m_worker_decode_enabled = false;
		}
		return true;
	}

	bool ResourceVideoImpl::Resume() {
		if (m_state == VideoPlaybackState::Paused) {
			m_state = VideoPlaybackState::Playing;
		}
		{
			std::lock_guard const lock(m_worker_mutex);
			m_worker_decode_enabled = true;
		}
		m_worker_cv.notify_one();
		return true;
	}

	bool ResourceVideoImpl::Stop() {
		m_state = VideoPlaybackState::Stopped;
		m_time = 0.0;
		{
			std::lock_guard const lock(m_worker_mutex);
			m_worker_decode_enabled = false;
		}
		requestWorkerSeek(0.0, true);
		return true;
	}

	bool ResourceVideoImpl::Seek(double const seconds) {
		m_time = normalize_time(seconds, m_duration);
		requestWorkerSeek(m_time, true);
		return true;
	}
}
