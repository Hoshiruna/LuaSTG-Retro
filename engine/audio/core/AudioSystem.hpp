#pragma once
#include "core/Data.hpp"
#include "core/ReferenceCounted.hpp"
#include <cstdint>
#include <string_view>

namespace core {
	enum class AudioBus : uint8_t {
		master,
		sound_effect,
		music,
	};

	enum class AudioLoadMode : uint8_t {
		decoded,
		streaming,
	};

	enum class AudioVoiceState : uint8_t {
		pending,
		playing,
		paused,
		stopped,
		ended,
		failed,
	};

	struct AudioFrameRange {
		uint64_t begin{};
		uint64_t end{}; // Exclusive. Zero means the end of the asset.
	};

	struct AudioVoiceHandle {
		uint32_t index{ UINT32_MAX };
		uint32_t generation{};

		[[nodiscard]] bool valid() const noexcept { return index != UINT32_MAX; }
		friend bool operator==(AudioVoiceHandle const&, AudioVoiceHandle const&) = default;
	};

	struct AudioPlaybackParameters {
		float volume{ 1.0f };
		float pan{};
		float pitch{ 1.0f };
		uint8_t priority{ 128 };
		bool loop{};
		bool enable_analyzer{};
		AudioFrameRange loop_range{};
	};

	struct AudioDeviceInfo {
		std::string_view id;
		std::string_view name;
		bool is_default{};
	};

	CORE_INTERFACE IAudioAsset : IReferenceCounted {
		[[nodiscard]] virtual std::string_view getPath() const noexcept = 0;
		[[nodiscard]] virtual AudioLoadMode getLoadMode() const noexcept = 0;
		[[nodiscard]] virtual AudioBus getBus() const noexcept = 0;
		[[nodiscard]] virtual uint64_t getFrameCount() const noexcept = 0;
		[[nodiscard]] virtual uint32_t getSampleRate() const noexcept = 0;
	};

	// UUID v5, namespace URL: https://www.luastg-sub.com/core.IAudioAsset
	template<> constexpr InterfaceId getInterfaceId<IAudioAsset>() { return UUID::parse("d7512d6b-79b0-5fba-9064-48ff51c64a24"); }

	CORE_INTERFACE IAudioSystem : IReferenceCounted {
		// Must be called from the game thread. Updates voice state and advances analyzers.
		virtual void update() = 0;

		[[nodiscard]] virtual bool refreshAudioDevices() = 0;
		[[nodiscard]] virtual uint32_t getAudioDeviceCount() const noexcept = 0;
		[[nodiscard]] virtual AudioDeviceInfo getAudioDevice(uint32_t index) const noexcept = 0;
		[[nodiscard]] virtual bool setAudioDevice(std::string_view id) = 0;
		[[nodiscard]] virtual std::string_view getCurrentAudioDeviceName() const noexcept = 0;
		[[nodiscard]] virtual bool isSilent() const noexcept = 0;

		virtual void setBusVolume(AudioBus bus, float volume) = 0;
		[[nodiscard]] virtual float getBusVolume(AudioBus bus) const noexcept = 0;
		virtual void setMaxSoundEffectVoices(uint32_t count) = 0;
		[[nodiscard]] virtual uint32_t getMaxSoundEffectVoices() const noexcept = 0;

		[[nodiscard]] virtual bool createAudioAsset(
			std::string_view path,
			IData* data,
			AudioLoadMode mode,
			AudioBus bus,
			IAudioAsset** output_asset) = 0;

		[[nodiscard]] virtual bool play(
			IAudioAsset* asset,
			AudioPlaybackParameters const& parameters,
			AudioVoiceHandle* output_voice) = 0;
		[[nodiscard]] virtual bool pause(AudioVoiceHandle voice) = 0;
		[[nodiscard]] virtual bool resume(AudioVoiceHandle voice) = 0;
		[[nodiscard]] virtual bool stop(AudioVoiceHandle voice) = 0;
		[[nodiscard]] virtual bool seek(AudioVoiceHandle voice, uint64_t frame) = 0;
		[[nodiscard]] virtual bool setLoopRange(AudioVoiceHandle voice, AudioFrameRange range) = 0;
		[[nodiscard]] virtual bool setLooping(AudioVoiceHandle voice, bool enabled) = 0;
		[[nodiscard]] virtual bool setVoiceVolume(AudioVoiceHandle voice, float volume) = 0;
		[[nodiscard]] virtual bool setVoicePan(AudioVoiceHandle voice, float pan) = 0;
		[[nodiscard]] virtual bool setVoicePitch(AudioVoiceHandle voice, float pitch) = 0;
		[[nodiscard]] virtual AudioVoiceState getVoiceState(AudioVoiceHandle voice) const noexcept = 0;
		[[nodiscard]] virtual uint64_t getVoiceCursor(AudioVoiceHandle voice) const noexcept = 0;
		[[nodiscard]] virtual uint64_t getVoiceLength(AudioVoiceHandle voice) const noexcept = 0;
		[[nodiscard]] virtual uint32_t getSpectrum(AudioVoiceHandle voice, float* output, uint32_t capacity) const noexcept = 0;

		[[nodiscard]] static bool create(IAudioSystem** output_system);
	};

	// UUID v5, namespace URL: https://www.luastg-sub.com/core.IAudioSystem
	template<> constexpr InterfaceId getInterfaceId<IAudioSystem>() { return UUID::parse("8bc4e618-ba9e-5af0-bd69-1a5d4ac0f6e3"); }
}
