#include "backend/AudioDecoderPMD.hpp"
#include "core/FileSystem.hpp"
#include "pmdwinimport.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace {
	template<typename T>
	bool ends_with_separator(T const& path) {
		if (path.empty()) {
			return false;
		}
		auto const c = path.back();
		return c == '\\' || c == '/';
	}
}

namespace core {
	AudioDecoderPMD::~AudioDecoderPMD() {
		shutdownEngine();
	}

	bool AudioDecoderPMD::isPMDFile(IData* const data) const {
		if (data == nullptr) {
			return false;
		}
		if (data->size() < 3) {
			return false;
		}
		auto const* const bytes = static_cast<uint8_t*>(data->data());
		if (bytes[0] > 0x0f) {
			return false;
		}
		if (!(bytes[1] == 0x18 || bytes[1] == 0x1a)) {
			return false;
		}
		if (bytes[2] != 0 && bytes[2] != 0xe6) {
			return false;
		}
		return true;
	}

	bool AudioDecoderPMD::initializeEngine() {
		std::scoped_lock lock(s_pmd_mutex);

		auto directory = m_directory.empty() ? std::filesystem::current_path() : m_directory;
		auto const directory_native = directory.lexically_normal().native();
		auto directory_string = directory_native;
		if (!ends_with_separator(directory_string)) {
			directory_string.push_back(std::filesystem::path::preferred_separator);
		}

		// init search path for extra PCM assets (PPS/PPZ/P86 etc.)
		std::array<TCHAR*, 2> pcm_paths{};
		pcm_paths[0] = const_cast<TCHAR*>(directory_string.c_str());
		pcm_paths[1] = nullptr;

		if (!pmdwininit(pcm_paths[0])) {
			Logger::error("[core] pmdwininit failed for '{}'", m_directory.string());
			return false;
		}

		setpcmdir(pcm_paths.data());

		m_sample_rate = SOUND_55K_2;
		setpcmrate(static_cast<int32_t>(m_sample_rate));
		// 55k modes output at 44.1k; report the real output rate to avoid speed drift.
		if (m_sample_rate == SOUND_55K || m_sample_rate == SOUND_55K_2) {
			m_sample_rate = SOUND_44K;
		}
		setppzrate(static_cast<int32_t>(m_sample_rate));
		setrhythmwithssgeffect(true);

		auto const* p = m_path.c_str();
		if (auto const result = music_load(const_cast<TCHAR*>(p)); result != PMDWIN_OK) {
			Logger::error("[core] music_load failed for '{}': {}", m_path.string(), result);
			return false;
		}

		music_start();

		int32_t length_ms{};
		int32_t loop_ms{};
		if (!getlength(const_cast<TCHAR*>(p), &length_ms, &loop_ms)) {
			Logger::warn("[core] getlength failed for '{}', fallback to 3 minutes", m_path.string());
			length_ms = 180'000; // 3 minutes fallback
		}
		m_total_samples = static_cast<uint32_t>((static_cast<uint64_t>(std::max(length_ms, 0)) * static_cast<uint64_t>(m_sample_rate)) / 1000ull);
		if (m_total_samples == 0) {
			m_total_samples = m_sample_rate * 60u; // ensure non-zero
		}

		s_active_decoder = this;
		m_initialized = true;
		m_current_frame = std::min(m_current_frame, m_total_samples);
		if (m_sample_rate != 0) {
			auto const ms = static_cast<int32_t>((static_cast<uint64_t>(m_current_frame) * 1000ull) / static_cast<uint64_t>(m_sample_rate));
			setpos(ms);
		}
		return true;
	}

	void AudioDecoderPMD::shutdownEngine() noexcept {
		std::scoped_lock lock(s_pmd_mutex);
		if (m_initialized) {
			music_stop();
			m_initialized = false;
		}
		if (s_active_decoder == this) {
			s_active_decoder = nullptr;
		}
		m_current_frame = 0;
		m_total_samples = 0;
		m_sample_rate = 0;
		m_data.reset();
	}

	bool AudioDecoderPMD::ensureActive() {
		if (!m_initialized) {
			return false;
		}
		if (s_active_decoder == this) {
			return true;
		}
		return initializeEngine();
	}

	bool AudioDecoderPMD::open(std::string_view const path, IData* const data) {
		if (!isPMDFile(data)) {
			return false;
		}

		shutdownEngine();

		m_data = data;
		m_path = std::filesystem::path(path).lexically_normal();
		m_directory = m_path.parent_path();

		return initializeEngine();
	}

	bool AudioDecoderPMD::seek(uint32_t const pcm_frame) {
		std::scoped_lock lock(s_pmd_mutex);
		if (!m_initialized || m_sample_rate == 0) {
			return false;
		}
		if (!ensureActive()) {
			return false;
		}
		m_current_frame = std::min(pcm_frame, m_total_samples);
		auto const ms = static_cast<int32_t>((static_cast<uint64_t>(m_current_frame) * 1000ull) / static_cast<uint64_t>(m_sample_rate));
		setpos(ms);
		return true;
	}

	bool AudioDecoderPMD::seekByTime(double const sec) {
		return seek(static_cast<uint32_t>(sec * static_cast<double>(m_sample_rate)));
	}

	bool AudioDecoderPMD::tell(uint32_t* const pcm_frame) {
		*pcm_frame = m_current_frame;
		return true;
	}

	bool AudioDecoderPMD::tellAsTime(double* const sec) {
		if (m_sample_rate == 0) {
			*sec = 0.0;
			return false;
		}
		*sec = static_cast<double>(m_current_frame) / static_cast<double>(m_sample_rate);
		return true;
	}

	bool AudioDecoderPMD::read(uint32_t const pcm_frame, void* const buffer, uint32_t* const read_pcm_frame) {
		std::scoped_lock lock(s_pmd_mutex);
		if (!m_initialized) {
			return false;
		}
		if (!ensureActive()) {
			return false;
		}
		auto const remaining = m_total_samples - std::min(m_current_frame, m_total_samples);
		auto const frames_to_read = std::min(pcm_frame, remaining);
		if (frames_to_read == 0) {
			if (read_pcm_frame) {
				*read_pcm_frame = 0;
			}
			return true;
		}
		getpcmdata(static_cast<int16_t*>(buffer), static_cast<int32_t>(frames_to_read));
		m_current_frame += frames_to_read;
		if (read_pcm_frame) {
			*read_pcm_frame = frames_to_read;
		}
		return true;
	}
}
