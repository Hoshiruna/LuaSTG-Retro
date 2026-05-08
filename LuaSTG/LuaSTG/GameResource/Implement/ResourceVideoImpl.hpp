#pragma once
#include "GameResource/ResourceVideo.hpp"
#include "GameResource/Implement/ResourceBaseImpl.hpp"
#include "core/VideoDecoder.hpp"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

namespace luastg {
	class ResourceVideoImpl final : public ResourceBaseImpl<IResourceVideo> {
	public:
		void Update(double delta_seconds) override;
		bool Play(bool restart) override;
		bool Pause() override;
		bool Resume() override;
		bool Stop() override;
		bool Seek(double seconds) override;
		VideoPlaybackState GetVideoState() const noexcept override { return m_state; }
		double GetVideoTime() const noexcept override { return m_time; }
		double GetVideoDuration() const noexcept override { return m_duration; }

		bool ResizeRenderTarget(core::Vector2U) override { return false; }
		core::Graphics::ITexture2D* GetTexture() override { return m_texture.get(); }
		core::Graphics::IRenderTarget* GetRenderTarget() override { return nullptr; }
		core::Graphics::IDepthStencilBuffer* GetDepthStencilBuffer() override { return nullptr; }
		bool IsRenderTarget() override { return false; }
		bool HasDepthStencilBuffer() override { return false; }

		ResourceVideoImpl(char const* name, char const* path, bool loop);
		~ResourceVideoImpl() override;

	private:
		bool uploadFrame(core::VideoFrame const& frame);
		bool uploadFrameAtCurrentTime();
		void requestWorkerSeek(double seconds, bool decode_one_frame);
		void workerMain();

	private:
		static constexpr size_t max_queued_frames = 4;

		core::SmartReference<core::IVideoDecoder> m_decoder;
		core::SmartReference<core::Graphics::ITexture2D> m_texture;
		bool m_loop{ false };
		VideoPlaybackState m_state{ VideoPlaybackState::Stopped };
		double m_time{ 0.0 };
		double m_duration{ 0.0 };

		std::thread m_worker;
		std::mutex m_worker_mutex;
		std::condition_variable m_worker_cv;
		std::deque<core::VideoFrame> m_frame_queue;
		bool m_worker_exit{ false };
		bool m_worker_decode_enabled{ false };
		bool m_worker_seek_requested{ false };
		bool m_worker_seek_decode_one_frame{ false };
		bool m_worker_eof{ false };
		bool m_worker_error{ false };
		bool m_force_upload_next_frame{ false };
		double m_worker_seek_time{ 0.0 };
		uint64_t m_worker_serial{ 0 };
	};
}
