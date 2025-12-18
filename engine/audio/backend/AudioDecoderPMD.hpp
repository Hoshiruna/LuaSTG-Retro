#pragma once
#include "core/AudioDecoder.hpp"
#include "core/SmartReference.hpp"
#include "core/Logger.hpp"
#include "core/implement/ReferenceCounted.hpp"
#include <mutex>
#include <string>
#include <filesystem>

namespace core {
	class AudioDecoderPMD final : public implement::ReferenceCounted<IAudioDecoder> {
	public:
		// IAudioDecoder

		[[nodiscard]] uint16_t getSampleSize() const noexcept override { return 2; }
		[[nodiscard]] uint16_t getChannelCount() const noexcept override { return 2; }
		[[nodiscard]] uint16_t getFrameSize() const noexcept override { return getChannelCount() * getSampleSize(); }
		[[nodiscard]] uint32_t getSampleRate() const noexcept override { return m_sample_rate; }
		[[nodiscard]] uint32_t getByteRate() const noexcept override { return getSampleRate() * static_cast<uint32_t>(getFrameSize()); }
		[[nodiscard]] uint32_t getFrameCount() const noexcept override { return m_total_samples; }

		[[nodiscard]] bool seek(uint32_t pcm_frame) override;
		[[nodiscard]] bool seekByTime(double sec) override;
		[[nodiscard]] bool tell(uint32_t* pcm_frame) override;
		[[nodiscard]] bool tellAsTime(double* sec) override;
		[[nodiscard]] bool read(uint32_t pcm_frame, void* buffer, uint32_t* read_pcm_frame) override;

		// AudioDecoderPMD

		AudioDecoderPMD() = default;
		AudioDecoderPMD(AudioDecoderPMD const&) = delete;
		AudioDecoderPMD(AudioDecoderPMD&&) = delete;
		~AudioDecoderPMD() override;

		AudioDecoderPMD& operator=(AudioDecoderPMD const&) = delete;
		AudioDecoderPMD& operator=(AudioDecoderPMD&&) = delete;

		[[nodiscard]] bool open(std::string_view path, IData* data);

	private:
		[[nodiscard]] bool isPMDFile(IData* data) const;
		[[nodiscard]] bool ensureActive();
		[[nodiscard]] bool initializeEngine();
		void shutdownEngine() noexcept;

		SmartReference<IData> m_data;
		std::filesystem::path m_path;
		std::filesystem::path m_directory;

		uint32_t m_sample_rate{ 0 };
		uint32_t m_total_samples{ 0 };
		uint32_t m_current_frame{ 0 };
		bool m_initialized{ false };

		inline static std::recursive_mutex s_pmd_mutex{};
		inline static AudioDecoderPMD* s_active_decoder{ nullptr };
	};
}
