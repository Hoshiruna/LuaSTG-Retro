#include "backend/VideoDecoderMediaFoundation.hpp"

#include "core/FileSystem.hpp"
#include "core/Logger.hpp"
#include "utf8.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mfapi.h>
#include <mutex>
#include <new>
#include <propvarutil.h>
#include <system_error>
#include <utility>

namespace {
	constexpr double hns_to_seconds(LONGLONG const value) noexcept {
		return static_cast<double>(value) / 10000000.0;
	}
	constexpr LONGLONG seconds_to_hns(double const value) noexcept {
		return static_cast<LONGLONG>(std::max(0.0, value) * 10000000.0);
	}

	HRESULT ensureMediaFoundationStarted() {
		static std::once_flag flag;
		static HRESULT result = E_FAIL;
		std::call_once(flag, [] {
			result = MFStartup(MF_VERSION, MFSTARTUP_FULL);
		});
		return result;
	}
	bool createSourceReaderAttributes(IMFAttributes** const attributes) {
		if (!attributes) {
			return false;
		}
		HRESULT const hr = MFCreateAttributes(attributes, 2);
		if (FAILED(hr)) {
			core::Logger::error("[core] MFCreateAttributes failed (HRESULT=0x{:08X})", static_cast<uint32_t>(hr));
			return false;
		}
		(*attributes)->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
		(*attributes)->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
		return true;
	}

	class DataStream final : public IStream {
	public:
		explicit DataStream(core::IData* data) : m_data(data) {}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
			if (!ppvObject) {
				return E_POINTER;
			}
			if (riid == IID_IUnknown || riid == IID_ISequentialStream || riid == IID_IStream) {
				*ppvObject = static_cast<IStream*>(this);
				AddRef();
				return S_OK;
			}
			*ppvObject = nullptr;
			return E_NOINTERFACE;
		}
		ULONG STDMETHODCALLTYPE AddRef() override {
			return m_refs.fetch_add(1) + 1;
		}
		ULONG STDMETHODCALLTYPE Release() override {
			ULONG const refs = m_refs.fetch_sub(1) - 1;
			if (refs == 0) {
				delete this;
			}
			return refs;
		}

		HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead) override {
			if (!pv && cb > 0) {
				return STG_E_INVALIDPOINTER;
			}
			size_t const available = m_position < m_data->size() ? m_data->size() - m_position : 0;
			size_t const read_size = std::min<size_t>(available, cb);
			if (read_size > 0) {
				std::memcpy(pv, static_cast<uint8_t const*>(m_data->data()) + m_position, read_size);
				m_position += read_size;
			}
			if (pcbRead) {
				*pcbRead = static_cast<ULONG>(read_size);
			}
			return read_size == cb ? S_OK : S_FALSE;
		}
		HRESULT STDMETHODCALLTYPE Write(void const*, ULONG, ULONG*) override {
			return STG_E_ACCESSDENIED;
		}
		HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) override {
			int64_t base = 0;
			switch (dwOrigin) {
			case STREAM_SEEK_SET:
				base = 0;
				break;
			case STREAM_SEEK_CUR:
				base = static_cast<int64_t>(m_position);
				break;
			case STREAM_SEEK_END:
				base = static_cast<int64_t>(m_data->size());
				break;
			default:
				return STG_E_INVALIDFUNCTION;
			}
			int64_t const next = base + dlibMove.QuadPart;
			if (next < 0) {
				return STG_E_INVALIDFUNCTION;
			}
			m_position = static_cast<size_t>(next);
			if (plibNewPosition) {
				plibNewPosition->QuadPart = m_position;
			}
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER) override {
			return STG_E_ACCESSDENIED;
		}
		HRESULT STDMETHODCALLTYPE CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) override {
			if (!pstm) {
				return STG_E_INVALIDPOINTER;
			}
			ULARGE_INTEGER total_read{};
			ULARGE_INTEGER total_written{};
			uint8_t buffer[4096]{};
			ULARGE_INTEGER remaining = cb;
			while (remaining.QuadPart > 0) {
				ULONG const request = static_cast<ULONG>(std::min<ULONGLONG>(remaining.QuadPart, sizeof(buffer)));
				ULONG read = 0;
				HRESULT hr = Read(buffer, request, &read);
				if (FAILED(hr)) {
					return hr;
				}
				if (read == 0) {
					break;
				}
				ULONG written = 0;
				hr = pstm->Write(buffer, read, &written);
				if (FAILED(hr)) {
					return hr;
				}
				total_read.QuadPart += read;
				total_written.QuadPart += written;
				remaining.QuadPart -= read;
				if (written != read) {
					break;
				}
			}
			if (pcbRead) {
				*pcbRead = total_read;
			}
			if (pcbWritten) {
				*pcbWritten = total_written;
			}
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE Commit(DWORD) override {
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE Revert() override {
			return STG_E_REVERTED;
		}
		HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override {
			return STG_E_INVALIDFUNCTION;
		}
		HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override {
			return STG_E_INVALIDFUNCTION;
		}
		HRESULT STDMETHODCALLTYPE Stat(STATSTG* pstatstg, DWORD) override {
			if (!pstatstg) {
				return STG_E_INVALIDPOINTER;
			}
			std::memset(pstatstg, 0, sizeof(*pstatstg));
			pstatstg->type = STGTY_STREAM;
			pstatstg->cbSize.QuadPart = m_data->size();
			pstatstg->grfMode = STGM_READ;
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE Clone(IStream** ppstm) override {
			if (!ppstm) {
				return STG_E_INVALIDPOINTER;
			}
			auto* stream = new (std::nothrow) DataStream(m_data.get());
			if (!stream) {
				*ppstm = nullptr;
				return E_OUTOFMEMORY;
			}
			stream->m_position = m_position;
			*ppstm = stream;
			return S_OK;
		}

	private:
		std::atomic<ULONG> m_refs{ 1 };
		core::SmartReference<core::IData> m_data;
		size_t m_position{};
	};
}

namespace core {
	bool VideoDecoderMediaFoundation::open(std::string_view const path) {
		m_source_data = nullptr;
		m_reader.Reset();
		m_width = 0;
		m_height = 0;
		m_stride = 0;
		m_duration = 0.0;

		if (FAILED(ensureMediaFoundationStarted())) {
			Logger::error("[core] Media Foundation startup failed");
			return false;
		}

		Microsoft::WRL::ComPtr<IMFAttributes> attributes;
		if (!createSourceReaderAttributes(attributes.GetAddressOf())) {
			return false;
		}

		std::string physical_path;
		if (FileSystemManager::resolvePhysicalPath(path, physical_path)) {
			if (!openFromPhysicalFile(physical_path, attributes.Get())) {
				Logger::warn("[core] failed to stream video file '{}'; falling back to memory stream", physical_path);
				m_reader.Reset();
			}
		}
		if (!m_reader && !openFromMemory(path, attributes.Get())) {
			return false;
		}

		m_reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
		HRESULT hr = m_reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
		if (FAILED(hr)) {
			Logger::error("[core] failed to select first video stream (HRESULT=0x{:08X})", static_cast<uint32_t>(hr));
			return false;
		}

		Microsoft::WRL::ComPtr<IMFMediaType> media_type;
		hr = MFCreateMediaType(media_type.GetAddressOf());
		if (FAILED(hr)) {
			return false;
		}
		media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
		media_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
		hr = m_reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, media_type.Get());
		if (FAILED(hr)) {
			Logger::error("[core] failed to request RGB32 video output (HRESULT=0x{:08X})", static_cast<uint32_t>(hr));
			return false;
		}
		if (!updateMediaType()) {
			return false;
		}

		PROPVARIANT duration;
		PropVariantInit(&duration);
		if (SUCCEEDED(m_reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &duration)) && duration.vt == VT_UI8) {
			m_duration = hns_to_seconds(static_cast<LONGLONG>(duration.uhVal.QuadPart));
		}
		PropVariantClear(&duration);

		return true;
	}

	bool VideoDecoderMediaFoundation::openFromPhysicalFile(std::string_view const path, IMFAttributes* const attributes) {
		std::filesystem::path file_path(utf8::to_wstring(path));
		std::error_code ec;
		std::filesystem::path const absolute_path = std::filesystem::absolute(file_path, ec);
		std::wstring const wide_path = (ec ? file_path : absolute_path).wstring();
		HRESULT const hr = MFCreateSourceReaderFromURL(wide_path.c_str(), attributes, m_reader.GetAddressOf());
		if (FAILED(hr)) {
			Logger::warn("[core] MFCreateSourceReaderFromURL failed (HRESULT=0x{:08X})", static_cast<uint32_t>(hr));
			return false;
		}
		m_source_data = nullptr;
		return true;
	}

	bool VideoDecoderMediaFoundation::openFromMemory(std::string_view const path, IMFAttributes* const attributes) {
		if (!FileSystemManager::readFile(path, m_source_data.put())) {
			Logger::error("[core] failed to read video file '{}'", path);
			return false;
		}

		Microsoft::WRL::ComPtr<IStream> stream;
		stream.Attach(new (std::nothrow) DataStream(m_source_data.get()));
		if (!stream) {
			return false;
		}

		Microsoft::WRL::ComPtr<IMFByteStream> byte_stream;
		HRESULT hr = MFCreateMFByteStreamOnStream(stream.Get(), byte_stream.GetAddressOf());
		if (FAILED(hr)) {
			Logger::error("[core] MFCreateMFByteStreamOnStream failed (HRESULT=0x{:08X})", static_cast<uint32_t>(hr));
			return false;
		}

		hr = MFCreateSourceReaderFromByteStream(byte_stream.Get(), attributes, m_reader.GetAddressOf());
		if (FAILED(hr)) {
			Logger::error("[core] MFCreateSourceReaderFromByteStream failed (HRESULT=0x{:08X})", static_cast<uint32_t>(hr));
			return false;
		}
		return true;
	}

	bool VideoDecoderMediaFoundation::updateMediaType() {
		Microsoft::WRL::ComPtr<IMFMediaType> media_type;
		HRESULT hr = m_reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, media_type.GetAddressOf());
		if (FAILED(hr)) {
			Logger::error("[core] failed to query current video media type (HRESULT=0x{:08X})", static_cast<uint32_t>(hr));
			return false;
		}
		UINT32 width = 0;
		UINT32 height = 0;
		hr = MFGetAttributeSize(media_type.Get(), MF_MT_FRAME_SIZE, &width, &height);
		if (FAILED(hr) || width == 0 || height == 0) {
			Logger::error("[core] failed to query video frame size (HRESULT=0x{:08X})", static_cast<uint32_t>(hr));
			return false;
		}
		m_width = width;
		m_height = height;
		UINT32 stride = 0;
		if (SUCCEEDED(media_type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride))) {
			m_stride = static_cast<int32_t>(stride);
		}
		else {
			m_stride = static_cast<int32_t>(m_width * 4);
		}
		return true;
	}

	bool VideoDecoderMediaFoundation::seek(double const seconds) {
		PROPVARIANT position;
		PropVariantInit(&position);
		position.vt = VT_I8;
		position.hVal.QuadPart = seconds_to_hns(seconds);
		HRESULT const hr = m_reader->SetCurrentPosition(GUID_NULL, position);
		PropVariantClear(&position);
		return SUCCEEDED(hr);
	}

	VideoDecodeResult VideoDecoderMediaFoundation::readFrame(VideoFrame& frame) {
		for (;;) {
			DWORD stream_index = 0;
			DWORD flags = 0;
			LONGLONG timestamp = 0;
			Microsoft::WRL::ComPtr<IMFSample> sample;
			HRESULT hr = m_reader->ReadSample(
				MF_SOURCE_READER_FIRST_VIDEO_STREAM,
				0,
				&stream_index,
				&flags,
				&timestamp,
				sample.GetAddressOf());
			if (FAILED(hr)) {
				Logger::error("[core] IMFSourceReader::ReadSample failed (HRESULT=0x{:08X})", static_cast<uint32_t>(hr));
				return VideoDecodeResult::Error;
			}
			if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) != 0) {
				if (!updateMediaType()) {
					return VideoDecodeResult::Error;
				}
			}
			if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
				return VideoDecodeResult::End;
			}
			if (!sample) {
				continue;
			}

			Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
			hr = sample->ConvertToContiguousBuffer(buffer.GetAddressOf());
			if (FAILED(hr)) {
				return VideoDecodeResult::Error;
			}

			BYTE* source = nullptr;
			DWORD max_length = 0;
			DWORD current_length = 0;
			hr = buffer->Lock(&source, &max_length, &current_length);
			if (FAILED(hr)) {
				return VideoDecodeResult::Error;
			}

			SmartReference<IData> pixels;
			if (!IData::create(static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * 4u, pixels.put())) {
				buffer->Unlock();
				return VideoDecodeResult::Error;
			}

			auto* destination = static_cast<uint8_t*>(pixels->data());
			uint32_t const destination_pitch = m_width * 4;
			int32_t const source_stride = m_stride == 0 ? static_cast<int32_t>(destination_pitch) : m_stride;
			uint32_t const source_pitch_abs = static_cast<uint32_t>(std::abs(source_stride));
			uint8_t const* row_source = source_stride >= 0
				? source
				: source + static_cast<size_t>(m_height - 1u) * source_pitch_abs;

			for (uint32_t y = 0; y < m_height; y += 1) {
				auto* const row_destination = destination + static_cast<size_t>(y) * destination_pitch;
				uint32_t const copy_pitch = std::min(destination_pitch, source_pitch_abs);
				std::memcpy(row_destination, row_source, copy_pitch);
				if (copy_pitch < destination_pitch) {
					std::memset(row_destination + copy_pitch, 0, destination_pitch - copy_pitch);
				}
				for (uint32_t x = 0; x < m_width; x += 1) {
					row_destination[static_cast<size_t>(x) * 4u + 3u] = 0xff;
				}
				if (source_stride >= 0) {
					row_source += source_pitch_abs;
				}
				else {
					row_source -= source_pitch_abs;
				}
			}

			buffer->Unlock();

			frame.pixels = std::move(pixels);
			frame.width = m_width;
			frame.height = m_height;
			frame.pitch = destination_pitch;
			frame.timestamp = hns_to_seconds(timestamp);
			return VideoDecodeResult::Frame;
		}
	}
}
