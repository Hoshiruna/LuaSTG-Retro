#include "backend/AudioEngineMiniaudio.hpp"
#include "core/Logger.hpp"
#include <miniaudio_libvorbis.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
#include <numbers>
#include <utility>

namespace core
{
    namespace
    {
        constexpr ma_uint32 output_sample_rate = 48000;
        constexpr ma_uint32 output_channels = 2;

        std::string makeDeviceId(ma_device_id const& id)
        {
            static constexpr char digits[] = "0123456789abcdef";
            auto const* bytes = reinterpret_cast<unsigned char const*>(&id);
            std::string result(sizeof(id) * 2, '0');
            for(size_t i = 0; i < sizeof(id); ++i) {
                result[i * 2] = digits[bytes[i] >> 4];
                result[i * 2 + 1] = digits[bytes[i] & 0x0f];
            }
            return result;
        }

        size_t busIndex(AudioBus const bus)
        {
            return static_cast<size_t>(bus);
        }
    }

    AudioAssetMiniaudio::AudioAssetMiniaudio(
        std::shared_ptr<AudioFileRegistry> registry,
        std::string path,
        std::string virtual_path,
        AudioLoadMode const mode,
        AudioBus const bus,
        uint64_t const frame_count,
        uint32_t const sample_rate)
        : m_registry(std::move(registry)), m_path(std::move(path)), m_virtual_path(std::move(virtual_path)), m_mode(mode), m_bus(bus), m_frame_count(frame_count), m_sample_rate(sample_rate)
    {
        std::lock_guard lock(m_registry->mutex);
        m_registry->assets.emplace(this);
    }

    AudioAssetMiniaudio::~AudioAssetMiniaudio()
    {
        detachFromEngine();
        std::lock_guard lock(m_registry->mutex);
        m_registry->assets.erase(this);
        m_registry->files.erase(m_virtual_path);
    }

    void AudioAssetMiniaudio::detachFromEngine() noexcept
    {
        if(m_prototype_initialized) {
            ma_sound_uninit(&m_prototype);
            m_prototype_initialized = false;
        }
    }

    AudioEngineMiniaudio::AudioEngineMiniaudio()
    {
        m_silent_last_update = std::chrono::steady_clock::now();
        m_file_registry = std::make_shared<AudioFileRegistry>();
        m_vfs.owner = this;
        m_vfs.callbacks.onOpen = vfsOpen;
        m_vfs.callbacks.onClose = vfsClose;
        m_vfs.callbacks.onRead = vfsRead;
        m_vfs.callbacks.onWrite = vfsWrite;
        m_vfs.callbacks.onSeek = vfsSeek;
        m_vfs.callbacks.onTell = vfsTell;
        m_vfs.callbacks.onInfo = vfsInfo;
    }

    AudioEngineMiniaudio::~AudioEngineMiniaudio()
    {
        closeDevice();
        for(auto& voice : m_voices) {
            retireVoice(voice);
        }
        std::vector<AudioAssetMiniaudio*> remaining_assets;
        {
            std::lock_guard lock(m_file_registry->mutex);
            remaining_assets.assign(m_file_registry->assets.begin(), m_file_registry->assets.end());
        }
        for(auto* const asset : remaining_assets) {
            asset->detachFromEngine();
        }
        if(m_music_group_initialized) {
            ma_sound_group_uninit(&m_music_group);
        }
        if(m_sfx_group_initialized) {
            ma_sound_group_uninit(&m_sfx_group);
        }
        if(m_engine_initialized) {
            ma_engine_uninit(&m_engine);
        }
        if(m_resource_manager_initialized) {
            ma_resource_manager_uninit(&m_resource_manager);
        }
        if(m_context_initialized) {
            ma_context_uninit(&m_context);
        }
    }

    bool AudioEngineMiniaudio::initialize()
    {
        if(ma_context_init(nullptr, 0, nullptr, &m_context) != MA_SUCCESS) {
            Logger::error("[audio] miniaudio context initialization failed");
            return false;
        }
        m_context_initialized = true;

        m_custom_decoders[0] = ma_decoding_backend_libvorbis;
        auto resource_config = ma_resource_manager_config_init();
        resource_config.pVFS = reinterpret_cast<ma_vfs*>(&m_vfs);
        resource_config.ppCustomDecodingBackendVTables = m_custom_decoders;
        resource_config.customDecodingBackendCount = 1;
        resource_config.decodedFormat = ma_format_f32;
        resource_config.decodedChannels = output_channels;
        resource_config.decodedSampleRate = output_sample_rate;
        resource_config.jobThreadCount = 2;
        if(ma_resource_manager_init(&resource_config, &m_resource_manager) != MA_SUCCESS) {
            Logger::error("[audio] miniaudio resource manager initialization failed");
            return false;
        }
        m_resource_manager_initialized = true;

        auto engine_config = ma_engine_config_init();
        engine_config.pResourceManager = &m_resource_manager;
        engine_config.noDevice = MA_TRUE;
        engine_config.channels = output_channels;
        engine_config.sampleRate = output_sample_rate;
        if(ma_engine_init(&engine_config, &m_engine) != MA_SUCCESS) {
            Logger::error("[audio] miniaudio engine initialization failed");
            return false;
        }
        m_engine_initialized = true;

        if(ma_sound_group_init(&m_engine, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &m_sfx_group) != MA_SUCCESS) {
            Logger::error("[audio] sound-effect bus initialization failed");
            return false;
        }
        m_sfx_group_initialized = true;
        if(ma_sound_group_init(&m_engine, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &m_music_group) != MA_SUCCESS) {
            Logger::error("[audio] music bus initialization failed");
            return false;
        }
        m_music_group_initialized = true;

        (void)refreshAudioDevices();
        if(!openDevice(nullptr)) {
            Logger::warn("[audio] no playback device is available; continuing in silent mode");
        }
        return true;
    }

    void AudioEngineMiniaudio::update()
    {
        auto const device_change = m_device_change_requested.exchange(0);
        if(device_change == 2) {
            closeDevice();
            (void)refreshAudioDevices();
            if(!openDevice(nullptr)) {
                Logger::warn("[audio] playback device was lost; continuing in silent mode");
            }
        } else if(device_change == 1 && m_device_initialized) {
            m_current_device_name = m_device.playback.name;
            (void)refreshAudioDevices();
        }
        auto const now = std::chrono::steady_clock::now();
        if(!m_device_initialized) {
            auto const elapsed = std::chrono::duration<double>(now - m_silent_last_update).count();
            auto frames_remaining = static_cast<ma_uint64>(std::clamp(elapsed, 0.0, 0.1) * output_sample_rate);
            std::array<float, 512 * output_channels> discard{};
            while(frames_remaining > 0) {
                auto const chunk = static_cast<ma_uint64>(std::min<ma_uint64>(frames_remaining, 512));
                ma_uint64 frames_read = 0;
                (void)ma_engine_read_pcm_frames(&m_engine, discard.data(), chunk, &frames_read);
                frames_remaining -= chunk;
            }
        }
        m_silent_last_update = now;

        for(auto& voice : m_voices) {
            if(!voice.initialized || voice.state == AudioVoiceState::stopped) {
                continue;
            }
            if(voice.stream_pending) {
                auto* const source = ma_sound_get_data_source(&voice.sound);
                auto const result = ma_resource_manager_data_source_result(reinterpret_cast<ma_resource_manager_data_source*>(source));
                if(result == MA_SUCCESS) {
                    voice.stream_pending = false;
                    if(voice.state == AudioVoiceState::pending)
                        voice.state = AudioVoiceState::playing;
                    if(voice.seek_pending) {
                        if(ma_sound_seek_to_pcm_frame(&voice.sound, voice.seek_frame) != MA_SUCCESS) {
                            voice.state = AudioVoiceState::failed;
                            (void)ma_sound_stop(&voice.sound);
                        } else {
                            voice.seek_pending = false;
                        }
                    }
                    if(voice.loop_range_pending) {
                        if(source == nullptr || ma_data_source_set_loop_point_in_pcm_frames(source, voice.loop_range.begin, voice.loop_range.end) != MA_SUCCESS) {
                            voice.state = AudioVoiceState::failed;
                            (void)ma_sound_stop(&voice.sound);
                        } else {
                            voice.loop_range_pending = false;
                        }
                    }
                } else if(result != MA_BUSY) {
                    voice.stream_pending = false;
                    voice.state = AudioVoiceState::failed;
                    (void)ma_sound_stop(&voice.sound);
                }
            }
            if(voice.state == AudioVoiceState::paused || voice.state == AudioVoiceState::failed) {
                continue;
            }
            if(ma_sound_at_end(&voice.sound)) {
                voice.state = AudioVoiceState::ended;
                continue;
            }
            if(voice.analyzer_decoder_initialized && voice.state == AudioVoiceState::playing) {
                constexpr ma_uint64 window_size = 512;
                std::array<float, window_size * output_channels> frames{};
                ma_uint64 cursor = 0;
                ma_sound_get_cursor_in_pcm_frames(&voice.sound, &cursor);
                ma_decoder_seek_to_pcm_frame(&voice.analyzer_decoder, cursor > window_size / 2 ? cursor - window_size / 2 : 0);
                ma_uint64 frames_read = 0;
                ma_decoder_read_pcm_frames(&voice.analyzer_decoder, frames.data(), window_size, &frames_read);
                for(uint32_t bin = 0; bin < spectrum_bin_count; ++bin) {
                    double real = 0.0;
                    double imaginary = 0.0;
                    for(ma_uint64 sample = 0; sample < frames_read; ++sample) {
                        auto const mono = 0.5 * static_cast<double>(frames[sample * 2] + frames[sample * 2 + 1]);
                        auto const window = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(sample) / static_cast<double>(window_size - 1));
                        auto const phase = -2.0 * std::numbers::pi * static_cast<double>(bin * sample) / static_cast<double>(window_size);
                        real += mono * window * std::cos(phase);
                        imaginary += mono * window * std::sin(phase);
                    }
                    voice.spectrum[bin] = static_cast<float>((2.0 / static_cast<double>(window_size)) * std::sqrt(real * real + imaginary * imaginary));
                }
            }
        }
    }

    bool AudioEngineMiniaudio::refreshAudioDevices()
    {
        ma_device_info* playback = nullptr;
        ma_uint32 playback_count = 0;
        if(ma_context_get_devices(&m_context, &playback, &playback_count, nullptr, nullptr) != MA_SUCCESS) {
            return false;
        }
        m_devices.clear();
        m_devices.reserve(playback_count);
        for(ma_uint32 i = 0; i < playback_count; ++i) {
            Device device;
            device.native_id = playback[i].id;
            device.id = makeDeviceId(playback[i].id);
            device.name = playback[i].name;
            device.is_default = playback[i].isDefault == MA_TRUE;
            m_devices.emplace_back(std::move(device));
        }
        return true;
    }

    uint32_t AudioEngineMiniaudio::getAudioDeviceCount() const noexcept
    {
        return static_cast<uint32_t>(m_devices.size());
    }

    AudioDeviceInfo AudioEngineMiniaudio::getAudioDevice(uint32_t const index) const noexcept
    {
        if(index >= m_devices.size()) {
            return {};
        }
        auto const& device = m_devices[index];
        return { device.id, device.name, device.is_default };
    }

    bool AudioEngineMiniaudio::setAudioDevice(std::string_view const id)
    {
        if(id.empty()) {
            closeDevice();
            return openDevice(nullptr);
        }
        for(auto const& device : m_devices) {
            if(device.id == id) {
                closeDevice();
                if(!openDevice(&device.native_id)) {
                    Logger::warn("[audio] failed to open '{}'; continuing in silent mode", device.name);
                    return false;
                }
                return true;
            }
        }
        return false;
    }

    std::string_view AudioEngineMiniaudio::getCurrentAudioDeviceName() const noexcept
    {
        return m_current_device_name;
    }

    void AudioEngineMiniaudio::setBusVolume(AudioBus const bus, float const volume)
    {
        auto const clamped = std::clamp(volume, 0.0f, 1.0f);
        m_bus_volumes[busIndex(bus)] = clamped;
        switch(bus) {
            case AudioBus::master: ma_engine_set_volume(&m_engine, clamped); break;
            case AudioBus::sound_effect: ma_sound_group_set_volume(&m_sfx_group, clamped); break;
            case AudioBus::music: ma_sound_group_set_volume(&m_music_group, clamped); break;
        }
    }

    float AudioEngineMiniaudio::getBusVolume(AudioBus const bus) const noexcept
    {
        return m_bus_volumes[busIndex(bus)];
    }

    void AudioEngineMiniaudio::setMaxSoundEffectVoices(uint32_t const count)
    {
        m_max_sfx_voices = std::clamp(count, 1u, max_sound_effect_capacity);
        for(;;) {
            uint32_t active = 0;
            uint32_t victim = UINT32_MAX;
            for(uint32_t i = 0; i < m_voices.size(); ++i) {
                auto const& voice = m_voices[i];
                if(!voice.initialized || voice.asset->getBus() != AudioBus::sound_effect ||
                    voice.state == AudioVoiceState::ended || voice.state == AudioVoiceState::failed || voice.state == AudioVoiceState::stopped) {
                    continue;
                }
                ++active;
                if(victim == UINT32_MAX || voice.priority < m_voices[victim].priority ||
                    (voice.priority == m_voices[victim].priority && voice.serial < m_voices[victim].serial)) {
                    victim = i;
                }
            }
            if(active <= m_max_sfx_voices || victim == UINT32_MAX)
                break;
            retireVoice(m_voices[victim]);
        }
    }

    bool AudioEngineMiniaudio::createAudioAsset(
        std::string_view const path,
        IData* const data,
        AudioLoadMode const mode,
        AudioBus const bus,
        IAudioAsset** const output_asset)
    {
        if(path.empty() || data == nullptr || output_asset == nullptr) {
            return false;
        }
        *output_asset = nullptr;
        try {

            auto const serial = m_asset_serial.fetch_add(1);
            auto const virtual_path = std::string("asset://") + std::to_string(serial) + "/" + std::string(path);
            auto asset = SmartReference<AudioAssetMiniaudio>();
            asset.attach(new AudioAssetMiniaudio(m_file_registry, std::string(path), virtual_path, mode, bus, 0, 0));
            {
                std::lock_guard lock(m_file_registry->mutex);
                m_file_registry->files.emplace(virtual_path, SmartReference<IData>(data));
            }

            ma_sound streaming_probe{};
            auto* const probe = mode == AudioLoadMode::decoded ? &asset->m_prototype : &streaming_probe;
            auto const flags = MA_SOUND_FLAG_NO_SPATIALIZATION |
                (mode == AudioLoadMode::decoded ? MA_SOUND_FLAG_DECODE : MA_SOUND_FLAG_STREAM);
            if(ma_sound_init_from_file(&m_engine, virtual_path.c_str(), flags, getGroup(bus), nullptr, probe) != MA_SUCCESS) {
                Logger::error("[audio] failed to decode '{}'", path);
                return false;
            }
            asset->m_prototype_initialized = mode == AudioLoadMode::decoded;

            ma_uint64 frame_count = 0;
            ma_uint32 sample_rate = 0;
            if(ma_sound_get_length_in_pcm_frames(probe, &frame_count) != MA_SUCCESS ||
                ma_sound_get_data_format(probe, nullptr, nullptr, &sample_rate, nullptr, 0) != MA_SUCCESS) {
                if(mode == AudioLoadMode::streaming)
                    ma_sound_uninit(probe);
                Logger::error("[audio] failed to inspect '{}'", path);
                return false;
            }

            asset->m_frame_count = frame_count;
            asset->m_sample_rate = sample_rate;
            if(mode == AudioLoadMode::streaming)
                ma_sound_uninit(probe);
            *output_asset = asset.detach();
            return true;
        } catch(std::exception const& error) {
            Logger::error("[audio] failed to create asset '{}': {}", path, error.what());
            return false;
        }
    }

    bool AudioEngineMiniaudio::play(
        IAudioAsset* const asset_interface,
        AudioPlaybackParameters const& parameters,
        AudioVoiceHandle* const output_voice)
    {
        if(asset_interface == nullptr || output_voice == nullptr) {
            return false;
        }
        *output_voice = {};
        auto* const asset = dynamic_cast<AudioAssetMiniaudio*>(asset_interface);
        if(asset == nullptr || asset->m_registry != m_file_registry) {
            return false;
        }
        auto const index = acquireVoice(asset, parameters.priority);
        if(index == UINT32_MAX) {
            return false;
        }
        auto& voice = m_voices[index];
        auto const common_flags = MA_SOUND_FLAG_NO_SPATIALIZATION;
        ma_result result;
        if(asset->getLoadMode() == AudioLoadMode::decoded) {
            result = ma_sound_init_copy(&m_engine, &asset->m_prototype, common_flags, getGroup(asset->getBus()), &voice.sound);
        } else {
            result = ma_sound_init_from_file(
                &m_engine,
                asset->getVirtualPath().c_str(),
                common_flags | MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_ASYNC,
                getGroup(asset->getBus()),
                nullptr,
                &voice.sound);
        }
        if(result != MA_SUCCESS) {
            retireVoice(voice);
            return false;
        }

        voice.initialized = true;
        voice.asset = asset;
        voice.priority = parameters.priority;
        voice.serial = ++m_voice_serial;
        voice.analyzer_enabled = parameters.enable_analyzer;
        voice.stream_pending = asset->getLoadMode() == AudioLoadMode::streaming;
        voice.state = voice.stream_pending ? AudioVoiceState::pending : AudioVoiceState::playing;
        ma_sound_set_volume(&voice.sound, std::max(0.0f, parameters.volume));
        ma_sound_set_pan(&voice.sound, std::clamp(parameters.pan, -1.0f, 1.0f));
        ma_sound_set_pitch(&voice.sound, std::max(0.01f, parameters.pitch));
        if(voice.analyzer_enabled) {
            auto decoder_config = ma_decoder_config_init(ma_format_f32, output_channels, output_sample_rate);
            decoder_config.ppCustomBackendVTables = m_custom_decoders;
            decoder_config.customBackendCount = 1;
            voice.analyzer_decoder_initialized = ma_decoder_init_vfs(
                                                     reinterpret_cast<ma_vfs*>(&m_vfs),
                                                     asset->getVirtualPath().c_str(),
                                                     &decoder_config,
                                                     &voice.analyzer_decoder) == MA_SUCCESS;
            if(!voice.analyzer_decoder_initialized)
                voice.analyzer_enabled = false;
        }
        if(parameters.loop) {
            if(!setLoopRange({ index, voice.generation }, parameters.loop_range)) {
                retireVoice(voice);
                return false;
            }
            ma_sound_set_looping(&voice.sound, MA_TRUE);
        }
        if(ma_sound_start(&voice.sound) != MA_SUCCESS) {
            retireVoice(voice);
            return false;
        }
        *output_voice = { index, voice.generation };
        return true;
    }

    bool AudioEngineMiniaudio::pause(AudioVoiceHandle const voice_handle)
    {
        auto* const voice = resolve(voice_handle);
        if(voice == nullptr || (voice->state != AudioVoiceState::playing && voice->state != AudioVoiceState::pending)) {
            return false;
        }
        if(ma_sound_stop(&voice->sound) != MA_SUCCESS) {
            return false;
        }
        voice->state = AudioVoiceState::paused;
        return true;
    }

    bool AudioEngineMiniaudio::resume(AudioVoiceHandle const voice_handle)
    {
        auto* const voice = resolve(voice_handle);
        if(voice == nullptr || voice->state != AudioVoiceState::paused) {
            return false;
        }
        if(ma_sound_start(&voice->sound) != MA_SUCCESS) {
            return false;
        }
        voice->state = voice->stream_pending ? AudioVoiceState::pending : AudioVoiceState::playing;
        return true;
    }

    bool AudioEngineMiniaudio::stop(AudioVoiceHandle const voice_handle)
    {
        auto* const voice = resolve(voice_handle);
        if(voice == nullptr) {
            return false;
        }
        ma_sound_stop(&voice->sound);
        ma_sound_seek_to_pcm_frame(&voice->sound, 0);
        voice->state = AudioVoiceState::stopped;
        return true;
    }

    bool AudioEngineMiniaudio::seek(AudioVoiceHandle const voice_handle, uint64_t const frame)
    {
        auto* const voice = resolve(voice_handle);
        if(voice == nullptr || frame > voice->asset->getFrameCount())
            return false;
        voice->seek_frame = frame;
        if(voice->stream_pending) {
            voice->seek_pending = true;
            return true;
        }
        voice->seek_pending = false;
        return ma_sound_seek_to_pcm_frame(&voice->sound, frame) == MA_SUCCESS;
    }

    bool AudioEngineMiniaudio::setLoopRange(AudioVoiceHandle const voice_handle, AudioFrameRange range)
    {
        auto* const voice = resolve(voice_handle);
        if(voice == nullptr) {
            return false;
        }
        auto const length = voice->asset->getFrameCount();
        if(range.end == 0) {
            range.end = length;
        }
        if(range.begin >= range.end || range.end > length) {
            return false;
        }
        voice->loop_range = range;
        if(voice->stream_pending) {
            voice->loop_range_pending = true;
            return true;
        }
        auto* const source = ma_sound_get_data_source(&voice->sound);
        voice->loop_range_pending = false;
        return source != nullptr && ma_data_source_set_loop_point_in_pcm_frames(source, range.begin, range.end) == MA_SUCCESS;
    }

    bool AudioEngineMiniaudio::setLooping(AudioVoiceHandle const voice_handle, bool const enabled)
    {
        auto* const voice = resolve(voice_handle);
        if(voice == nullptr)
            return false;
        ma_sound_set_looping(&voice->sound, enabled ? MA_TRUE : MA_FALSE);
        return true;
    }

    bool AudioEngineMiniaudio::setVoiceVolume(AudioVoiceHandle const voice_handle, float const volume)
    {
        auto* const voice = resolve(voice_handle);
        if(voice == nullptr)
            return false;
        ma_sound_set_volume(&voice->sound, std::max(0.0f, volume));
        return true;
    }

    bool AudioEngineMiniaudio::setVoicePan(AudioVoiceHandle const voice_handle, float const pan)
    {
        auto* const voice = resolve(voice_handle);
        if(voice == nullptr)
            return false;
        ma_sound_set_pan(&voice->sound, std::clamp(pan, -1.0f, 1.0f));
        return true;
    }

    bool AudioEngineMiniaudio::setVoicePitch(AudioVoiceHandle const voice_handle, float const pitch)
    {
        auto* const voice = resolve(voice_handle);
        if(voice == nullptr || pitch <= 0.0f)
            return false;
        ma_sound_set_pitch(&voice->sound, pitch);
        return true;
    }

    AudioVoiceState AudioEngineMiniaudio::getVoiceState(AudioVoiceHandle const voice_handle) const noexcept
    {
        auto const* const voice = resolve(voice_handle);
        return voice == nullptr ? AudioVoiceState::stopped : voice->state;
    }

    uint64_t AudioEngineMiniaudio::getVoiceCursor(AudioVoiceHandle const voice_handle) const noexcept
    {
        auto const* const voice = resolve(voice_handle);
        ma_uint64 cursor = 0;
        if(voice != nullptr) {
            ma_sound_get_cursor_in_pcm_frames(&voice->sound, &cursor);
        }
        return cursor;
    }

    uint64_t AudioEngineMiniaudio::getVoiceLength(AudioVoiceHandle const voice_handle) const noexcept
    {
        auto const* const voice = resolve(voice_handle);
        return voice == nullptr ? 0 : voice->asset->getFrameCount();
    }

    uint32_t AudioEngineMiniaudio::getSpectrum(AudioVoiceHandle const voice_handle, float* const output, uint32_t const capacity) const noexcept
    {
        auto const* const voice = resolve(voice_handle);
        if(voice == nullptr || !voice->analyzer_enabled || output == nullptr) {
            return 0;
        }
        auto const count = std::min(capacity, spectrum_bin_count);
        std::copy_n(voice->spectrum.begin(), count, output);
        return count;
    }

    void AudioEngineMiniaudio::dataCallback(ma_device* const device, void* const output, void const*, ma_uint32 const frame_count)
    {
        auto* const self = static_cast<AudioEngineMiniaudio*>(device->pUserData);
        ma_uint64 frames_read = 0;
        if(self == nullptr || ma_engine_read_pcm_frames(&self->m_engine, output, frame_count, &frames_read) != MA_SUCCESS) {
            std::memset(output, 0, static_cast<size_t>(frame_count) * output_channels * sizeof(float));
        }
    }

    void AudioEngineMiniaudio::deviceNotificationCallback(ma_device_notification const* const notification)
    {
        if(notification == nullptr || notification->pDevice == nullptr)
            return;
        auto* const self = static_cast<AudioEngineMiniaudio*>(notification->pDevice->pUserData);
        if(self == nullptr)
            return;
        if(notification->type == ma_device_notification_type_stopped && !self->m_ignore_device_stop.load()) {
            self->m_device_change_requested.store(2);
        } else if(notification->type == ma_device_notification_type_rerouted && self->m_device_change_requested.load() == 0) {
            self->m_device_change_requested.store(1);
        }
    }

    ma_result AudioEngineMiniaudio::vfsOpen(ma_vfs* const vfs, char const* const path, ma_uint32 const mode, ma_vfs_file* const file)
    {
        if((mode & MA_OPEN_MODE_READ) == 0 || path == nullptr || file == nullptr) {
            return MA_INVALID_ARGS;
        }
        auto* const self = reinterpret_cast<VirtualFileSystem*>(vfs)->owner;
        std::lock_guard lock(self->m_file_registry->mutex);
        auto const it = self->m_file_registry->files.find(path);
        if(it == self->m_file_registry->files.end()) {
            return MA_DOES_NOT_EXIST;
        }
        auto* handle = new(std::nothrow) MemoryFile;
        if(handle == nullptr) {
            return MA_OUT_OF_MEMORY;
        }
        handle->data = it->second;
        *file = reinterpret_cast<ma_vfs_file>(handle);
        return MA_SUCCESS;
    }

    ma_result AudioEngineMiniaudio::vfsClose(ma_vfs*, ma_vfs_file const file)
    {
        delete reinterpret_cast<MemoryFile*>(file);
        return MA_SUCCESS;
    }

    ma_result AudioEngineMiniaudio::vfsRead(ma_vfs*, ma_vfs_file const file, void* const output, size_t const bytes, size_t* const bytes_read)
    {
        auto* const handle = reinterpret_cast<MemoryFile*>(file);
        if(handle == nullptr || output == nullptr)
            return MA_INVALID_ARGS;
        auto const available = handle->data->size() - std::min(handle->cursor, handle->data->size());
        auto const count = std::min(bytes, available);
        std::memcpy(output, static_cast<std::byte const*>(handle->data->data()) + handle->cursor, count);
        handle->cursor += count;
        if(bytes_read != nullptr)
            *bytes_read = count;
        return count == 0 && bytes != 0 ? MA_AT_END : MA_SUCCESS;
    }

    ma_result AudioEngineMiniaudio::vfsWrite(ma_vfs*, ma_vfs_file, void const*, size_t, size_t* const bytes_written)
    {
        if(bytes_written != nullptr)
            *bytes_written = 0;
        return MA_NOT_IMPLEMENTED;
    }

    ma_result AudioEngineMiniaudio::vfsSeek(ma_vfs*, ma_vfs_file const file, ma_int64 const offset, ma_seek_origin const origin)
    {
        auto* const handle = reinterpret_cast<MemoryFile*>(file);
        if(handle == nullptr)
            return MA_INVALID_ARGS;
        auto const maximum = static_cast<uint64_t>(std::numeric_limits<ma_int64>::max());
        auto const cursor = static_cast<uint64_t>(handle->cursor);
        auto const size = static_cast<uint64_t>(handle->data->size());
        ma_int64 base = 0;
        switch(origin) {
            case ma_seek_origin_start: base = 0; break;
            case ma_seek_origin_current:
                if(cursor > maximum)
                    return MA_BAD_SEEK;
                base = static_cast<ma_int64>(cursor);
                break;
            case ma_seek_origin_end:
                if(size > maximum)
                    return MA_BAD_SEEK;
                base = static_cast<ma_int64>(size);
                break;
            default: return MA_INVALID_ARGS;
        }
        ma_int64 target = base;
        if(offset < 0) {
            // Avoid negating INT64_MIN while calculating the absolute offset.
            auto const magnitude = static_cast<uint64_t>(-(offset + 1)) + 1;
            if(magnitude > static_cast<uint64_t>(base))
                return MA_BAD_SEEK;
            target = base - static_cast<ma_int64>(magnitude);
        } else {
            if(base > std::numeric_limits<ma_int64>::max() - offset)
                return MA_BAD_SEEK;
            target = base + offset;
        }
        if(static_cast<uint64_t>(target) > size) {
            return MA_BAD_SEEK;
        }
        handle->cursor = static_cast<size_t>(target);
        return MA_SUCCESS;
    }

    ma_result AudioEngineMiniaudio::vfsTell(ma_vfs*, ma_vfs_file const file, ma_int64* const cursor)
    {
        auto const* const handle = reinterpret_cast<MemoryFile*>(file);
        if(handle == nullptr || cursor == nullptr)
            return MA_INVALID_ARGS;
        if(static_cast<uint64_t>(handle->cursor) > static_cast<uint64_t>(std::numeric_limits<ma_int64>::max())) {
            return MA_BAD_SEEK;
        }
        *cursor = static_cast<ma_int64>(handle->cursor);
        return MA_SUCCESS;
    }

    ma_result AudioEngineMiniaudio::vfsInfo(ma_vfs*, ma_vfs_file const file, ma_file_info* const info)
    {
        auto const* const handle = reinterpret_cast<MemoryFile*>(file);
        if(handle == nullptr || info == nullptr)
            return MA_INVALID_ARGS;
        info->sizeInBytes = handle->data->size();
        return MA_SUCCESS;
    }

    AudioEngineMiniaudio::Voice* AudioEngineMiniaudio::resolve(AudioVoiceHandle const voice) noexcept
    {
        if(voice.index >= m_voices.size())
            return nullptr;
        auto& slot = m_voices[voice.index];
        return slot.initialized && slot.generation == voice.generation ? &slot : nullptr;
    }

    AudioEngineMiniaudio::Voice const* AudioEngineMiniaudio::resolve(AudioVoiceHandle const voice) const noexcept
    {
        if(voice.index >= m_voices.size())
            return nullptr;
        auto const& slot = m_voices[voice.index];
        return slot.initialized && slot.generation == voice.generation ? &slot : nullptr;
    }

    uint32_t AudioEngineMiniaudio::acquireVoice(AudioAssetMiniaudio* const asset, uint8_t const priority)
    {
        uint32_t active_sfx = 0;
        uint32_t free_slot = UINT32_MAX;
        for(uint32_t i = 0; i < m_voices.size(); ++i) {
            auto const& voice = m_voices[i];
            if(!voice.initialized || voice.state == AudioVoiceState::ended || voice.state == AudioVoiceState::failed || voice.state == AudioVoiceState::stopped) {
                if(free_slot == UINT32_MAX)
                    free_slot = i;
            } else if(voice.asset->getBus() == AudioBus::sound_effect) {
                ++active_sfx;
            }
        }

        bool const needs_steal = free_slot == UINT32_MAX ||
            (asset->getBus() == AudioBus::sound_effect && active_sfx >= m_max_sfx_voices);
        uint32_t selected = free_slot;
        if(needs_steal) {
            selected = UINT32_MAX;
            for(uint32_t i = 0; i < m_voices.size(); ++i) {
                auto const& candidate = m_voices[i];
                if(!candidate.initialized || candidate.priority > priority)
                    continue;
                if(asset->getBus() == AudioBus::sound_effect && candidate.asset->getBus() != AudioBus::sound_effect)
                    continue;
                if(selected == UINT32_MAX || candidate.priority < m_voices[selected].priority ||
                    (candidate.priority == m_voices[selected].priority && candidate.serial < m_voices[selected].serial)) {
                    selected = i;
                }
            }
        }
        if(selected == UINT32_MAX)
            return selected;
        retireVoice(m_voices[selected]);
        auto& generation = m_voices[selected].generation;
        if(++generation == 0)
            generation = 1;
        return selected;
    }

    void AudioEngineMiniaudio::retireVoice(Voice& voice)
    {
        if(voice.analyzer_decoder_initialized) {
            ma_decoder_uninit(&voice.analyzer_decoder);
        }
        if(voice.initialized) {
            ma_sound_uninit(&voice.sound);
        }
        voice.initialized = false;
        voice.asset.reset();
        voice.seek_frame = 0;
        voice.loop_range = {};
        voice.state = AudioVoiceState::stopped;
        voice.stream_pending = false;
        voice.seek_pending = false;
        voice.loop_range_pending = false;
        voice.analyzer_enabled = false;
        voice.analyzer_decoder_initialized = false;
        voice.spectrum.fill(0.0f);
    }

    bool AudioEngineMiniaudio::openDevice(ma_device_id const* const id)
    {
        auto config = ma_device_config_init(ma_device_type_playback);
        config.playback.pDeviceID = id;
        config.playback.format = ma_format_f32;
        config.playback.channels = output_channels;
        config.sampleRate = output_sample_rate;
        config.dataCallback = dataCallback;
        config.notificationCallback = deviceNotificationCallback;
        config.pUserData = this;
        if(ma_device_init(&m_context, &config, &m_device) != MA_SUCCESS) {
            return false;
        }
        m_device_initialized = true;
        m_device_change_requested.store(0);
        m_current_device_name = m_device.playback.name;
        if(ma_device_start(&m_device) != MA_SUCCESS) {
            closeDevice();
            return false;
        }
        return true;
    }

    void AudioEngineMiniaudio::closeDevice()
    {
        if(m_device_initialized) {
            m_ignore_device_stop.store(true);
            ma_device_uninit(&m_device);
            m_ignore_device_stop.store(false);
            m_device_initialized = false;
        }
        m_current_device_name.clear();
    }

    ma_sound_group* AudioEngineMiniaudio::getGroup(AudioBus const bus) noexcept
    {
        switch(bus) {
            case AudioBus::sound_effect: return &m_sfx_group;
            case AudioBus::music: return &m_music_group;
            case AudioBus::master: return nullptr;
        }
        return nullptr;
    }

    bool IAudioSystem::create(IAudioSystem** const output_system)
    {
        if(output_system == nullptr)
            return false;
        *output_system = nullptr;
        try {
            auto engine = SmartReference<AudioEngineMiniaudio>();
            engine.attach(new AudioEngineMiniaudio);
            if(!engine->initialize()) {
                return false;
            }
            *output_system = engine.detach();
            return true;
        } catch(std::exception const& error) {
            Logger::error("[audio] audio system creation failed: {}", error.what());
            return false;
        }
    }
}
