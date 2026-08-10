#pragma once
#include "core/AudioSystem.hpp"
#include "core/SmartReference.hpp"
#include "core/implement/ReferenceCounted.hpp"
#include <miniaudio.h>
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace core
{
    class AudioEngineMiniaudio;
    class AudioAssetMiniaudio;
    struct AudioFileRegistry
    {
        std::mutex mutex;
        std::unordered_map<std::string, SmartReference<IData>> files;
        std::unordered_set<AudioAssetMiniaudio*> assets;
    };

    class AudioAssetMiniaudio final : public implement::ReferenceCounted<IAudioAsset>
    {
    public:
        AudioAssetMiniaudio(
            std::shared_ptr<AudioFileRegistry> registry,
            std::string path,
            std::string virtual_path,
            AudioLoadMode mode,
            AudioBus bus,
            uint64_t frame_count,
            uint32_t sample_rate);
        ~AudioAssetMiniaudio() override;

        [[nodiscard]] std::string_view getPath() const noexcept override { return m_path; }
        [[nodiscard]] AudioLoadMode getLoadMode() const noexcept override { return m_mode; }
        [[nodiscard]] AudioBus getBus() const noexcept override { return m_bus; }
        [[nodiscard]] uint64_t getFrameCount() const noexcept override { return m_frame_count; }
        [[nodiscard]] uint32_t getSampleRate() const noexcept override { return m_sample_rate; }
        [[nodiscard]] std::string const& getVirtualPath() const noexcept { return m_virtual_path; }

    private:
        friend class AudioEngineMiniaudio;
        void detachFromEngine() noexcept;
        std::shared_ptr<AudioFileRegistry> m_registry;
        std::string m_path;
        std::string m_virtual_path;
        AudioLoadMode m_mode;
        AudioBus m_bus;
        uint64_t m_frame_count;
        uint32_t m_sample_rate;
        ma_sound m_prototype{};
        bool m_prototype_initialized{};
    };

    class AudioEngineMiniaudio final : public implement::ReferenceCounted<IAudioSystem>
    {
    public:
        AudioEngineMiniaudio();
        ~AudioEngineMiniaudio() override;

        void update() override;
        [[nodiscard]] bool refreshAudioDevices() override;
        [[nodiscard]] uint32_t getAudioDeviceCount() const noexcept override;
        [[nodiscard]] AudioDeviceInfo getAudioDevice(uint32_t index) const noexcept override;
        [[nodiscard]] bool setAudioDevice(std::string_view id) override;
        [[nodiscard]] std::string_view getCurrentAudioDeviceName() const noexcept override;
        [[nodiscard]] bool isSilent() const noexcept override { return !m_device_initialized; }

        void setBusVolume(AudioBus bus, float volume) override;
        [[nodiscard]] float getBusVolume(AudioBus bus) const noexcept override;
        void setMaxSoundEffectVoices(uint32_t count) override;
        [[nodiscard]] uint32_t getMaxSoundEffectVoices() const noexcept override { return m_max_sfx_voices; }

        [[nodiscard]] bool createAudioAsset(std::string_view path, IData* data, AudioLoadMode mode, AudioBus bus, IAudioAsset** output_asset) override;
        [[nodiscard]] bool play(IAudioAsset* asset, AudioPlaybackParameters const& parameters, AudioVoiceHandle* output_voice) override;
        [[nodiscard]] bool pause(AudioVoiceHandle voice) override;
        [[nodiscard]] bool resume(AudioVoiceHandle voice) override;
        [[nodiscard]] bool stop(AudioVoiceHandle voice) override;
        [[nodiscard]] bool seek(AudioVoiceHandle voice, uint64_t frame) override;
        [[nodiscard]] bool setLoopRange(AudioVoiceHandle voice, AudioFrameRange range) override;
        [[nodiscard]] bool setLooping(AudioVoiceHandle voice, bool enabled) override;
        [[nodiscard]] bool setVoiceVolume(AudioVoiceHandle voice, float volume) override;
        [[nodiscard]] bool setVoicePan(AudioVoiceHandle voice, float pan) override;
        [[nodiscard]] bool setVoicePitch(AudioVoiceHandle voice, float pitch) override;
        [[nodiscard]] AudioVoiceState getVoiceState(AudioVoiceHandle voice) const noexcept override;
        [[nodiscard]] uint64_t getVoiceCursor(AudioVoiceHandle voice) const noexcept override;
        [[nodiscard]] uint64_t getVoiceLength(AudioVoiceHandle voice) const noexcept override;
        [[nodiscard]] uint32_t getSpectrum(AudioVoiceHandle voice, float* output, uint32_t capacity) const noexcept override;

        [[nodiscard]] bool initialize();

    private:
        static constexpr uint32_t max_sound_effect_capacity = 256;
        static constexpr uint32_t max_voice_capacity = max_sound_effect_capacity + 16;
        static constexpr uint32_t spectrum_bin_count = 256;

        struct MemoryFile
        {
            SmartReference<IData> data;
            size_t cursor{};
        };
        struct VirtualFileSystem
        {
            ma_vfs_callbacks callbacks{};
            AudioEngineMiniaudio* owner{};
        };
        struct Device
        {
            ma_device_id native_id{};
            std::string id;
            std::string name;
            bool is_default{};
        };
        struct Voice
        {
            ma_sound sound{};
            ma_decoder analyzer_decoder{};
            SmartReference<AudioAssetMiniaudio> asset;
            std::array<float, spectrum_bin_count> spectrum{};
            uint64_t serial{};
            uint64_t seek_frame{};
            uint32_t generation{ 1 };
            AudioFrameRange loop_range{};
            AudioVoiceState state{ AudioVoiceState::stopped };
            uint8_t priority{};
            bool initialized{};
            bool stream_pending{};
            bool seek_pending{};
            bool loop_range_pending{};
            bool analyzer_enabled{};
            bool analyzer_decoder_initialized{};
        };

        static void dataCallback(ma_device* device, void* output, void const* input, ma_uint32 frame_count);
        static void deviceNotificationCallback(ma_device_notification const* notification);
        static ma_result vfsOpen(ma_vfs* vfs, char const* path, ma_uint32 mode, ma_vfs_file* file);
        static ma_result vfsClose(ma_vfs* vfs, ma_vfs_file file);
        static ma_result vfsRead(ma_vfs* vfs, ma_vfs_file file, void* output, size_t bytes, size_t* bytes_read);
        static ma_result vfsWrite(ma_vfs* vfs, ma_vfs_file file, void const* input, size_t bytes, size_t* bytes_written);
        static ma_result vfsSeek(ma_vfs* vfs, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin);
        static ma_result vfsTell(ma_vfs* vfs, ma_vfs_file file, ma_int64* cursor);
        static ma_result vfsInfo(ma_vfs* vfs, ma_vfs_file file, ma_file_info* info);

        [[nodiscard]] Voice* resolve(AudioVoiceHandle voice) noexcept;
        [[nodiscard]] Voice const* resolve(AudioVoiceHandle voice) const noexcept;
        [[nodiscard]] uint32_t acquireVoice(AudioAssetMiniaudio* asset, uint8_t priority);
        void retireVoice(Voice& voice);
        [[nodiscard]] bool openDevice(ma_device_id const* id);
        void closeDevice();
        [[nodiscard]] ma_sound_group* getGroup(AudioBus bus) noexcept;

        VirtualFileSystem m_vfs;
        std::shared_ptr<AudioFileRegistry> m_file_registry;
        std::atomic_uint64_t m_asset_serial{ 1 };

        ma_context m_context{};
        ma_resource_manager m_resource_manager{};
        ma_engine m_engine{};
        ma_sound_group m_sfx_group{};
        ma_sound_group m_music_group{};
        ma_device m_device{};
        ma_decoding_backend_vtable* m_custom_decoders[1]{};
        bool m_context_initialized{};
        bool m_resource_manager_initialized{};
        bool m_engine_initialized{};
        bool m_sfx_group_initialized{};
        bool m_music_group_initialized{};
        bool m_device_initialized{};
        std::atomic_int m_device_change_requested{};
        std::atomic_bool m_ignore_device_stop{};
        std::vector<Device> m_devices;
        std::string m_current_device_name;
        std::array<Voice, max_voice_capacity> m_voices;
        std::array<float, 3> m_bus_volumes{ 1.0f, 1.0f, 1.0f };
        uint32_t m_max_sfx_voices{ max_sound_effect_capacity };
        uint64_t m_voice_serial{};
        std::chrono::steady_clock::time_point m_silent_last_update;
    };
}
