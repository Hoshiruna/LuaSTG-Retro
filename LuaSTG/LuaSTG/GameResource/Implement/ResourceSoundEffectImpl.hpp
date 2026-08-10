#pragma once
<<<<<<< HEAD
#include "GameResource/Implement/ResourceBaseImpl.hpp"
#include "GameResource/ResourceSoundEffect.hpp"
#include "core/SmartReference.hpp"

namespace luastg {
	class ResourceSoundEffectImpl final : public ResourceBaseImpl<IResourceSoundEffect> {
	public:
		ResourceSoundEffectImpl(char const* name, core::IAudioAsset* asset);
		[[nodiscard]] core::IAudioAsset* GetAudioAsset() const noexcept override { return m_asset.get(); }

	private:
		core::SmartReference<core::IAudioAsset> m_asset;
	};
=======
#include "core/SmartReference.hpp"
#include "GameResource/ResourceSoundEffect.hpp"
#include "GameResource/Implement/ResourceBaseImpl.hpp"
#include "core/AudioPlayer.hpp"

namespace luastg
{
    class ResourceSoundEffectImpl final : public ResourceBaseImpl<IResourceSoundEffect>
    {
    public:
        void FlushCommand() override;
        void Play(float vol, float pan) override;
        void Resume() override;
        void Pause() override;
        void Stop() override;
        bool IsPlaying() override;
        bool IsStopped() override;
        bool SetSpeed(float speed) override;
        float GetSpeed() override;

        ResourceSoundEffectImpl(const char* name, core::IAudioPlayer* p_player);

    private:
        enum class CommandType : uint8_t
        {
            none,
            play,
            pause,
            resume,
            stop,
        };

        struct Command
        {
            CommandType type{ CommandType::none };
            float vol{ 1.0f };
            float pan{ 0.0f };
        };

        core::SmartReference<core::IAudioPlayer> m_player;
        core::AudioPlayerState m_state{ core::AudioPlayerState::stopped };
        Command m_last_command;
    };
>>>>>>> origin/master
}
