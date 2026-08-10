#include <iostream>
#include <print>
#include "core/AudioSystem.hpp"
#include "core/FileSystem.hpp"
#include "core/Logger.hpp"
#include "core/SmartReference.hpp"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#ifndef NDEBUG
#include "core/implement/ReferenceCountedDebugger.hpp"
#endif

using std::string_view_literals::operator""sv;

namespace
{
    core::SmartReference<core::IAudioSystem> audio;
    core::SmartReference<core::IAudioAsset> asset;
    core::AudioVoiceHandle voice;

    void listDevices()
    {
        if(!audio->refreshAudioDevices())
            return;
        for(uint32_t i = 0; i < audio->getAudioDeviceCount(); ++i) {
            auto const device = audio->getAudioDevice(i);
            std::println("{}. {} [{}]{}", i, device.name, device.id, device.is_default ? " (default)" : "");
        }
    }

    void openAsset(std::string const& command)
    {
        auto const begin = command.find_first_of('"');
        auto const end = command.find_last_of('"');
        if(begin == std::string::npos || begin == end) {
            core::Logger::error("expected a quoted path");
            return;
        }
        auto const path = std::string_view(command).substr(begin + 1, end - begin - 1);
        core::SmartReference<core::IData> data;
        if(!core::FileSystemManager::readFile(path, data.put()) ||
            !audio->createAudioAsset(path, data.get(), core::AudioLoadMode::streaming, core::AudioBus::music, asset.put())) {
            core::Logger::error("failed to open '{}'", path);
            return;
        }
        std::println("'{}' opened ({} frames at {} Hz)", path, asset->getFrameCount(), asset->getSampleRate());
    }

    int run()
    {
        if(!core::IAudioSystem::create(audio.put()))
            return 1;
        std::println("Commands: devices, device <index>, open \"path\", play, pause, resume, stop, state, exit");
        std::string command;
        while(true) {
            audio->update();
            std::print("> ");
            std::cin >> command;
            if(command == "exit"sv)
                break;
            if(command == "devices"sv)
                listDevices();
            else if(command == "device"sv) {
                uint32_t index{};
                std::cin >> index;
                if(index < audio->getAudioDeviceCount())
                    audio->setAudioDevice(audio->getAudioDevice(index).id);
            } else if(command == "open"sv) {
                std::getline(std::cin, command);
                openAsset(command);
            } else if(command == "play"sv && asset) {
                core::AudioPlaybackParameters parameters;
                parameters.loop = true;
                parameters.loop_range.end = asset->getFrameCount();
                audio->play(asset.get(), parameters, &voice);
            } else if(command == "pause"sv)
                audio->pause(voice);
            else if(command == "resume"sv)
                audio->resume(voice);
            else if(command == "stop"sv)
                audio->stop(voice);
            else if(command == "state"sv)
                std::println("state = {}", static_cast<int>(audio->getVoiceState(voice)));
        }
        asset.reset();
        audio.reset();
        return 0;
    }
}

int
main()
{
    auto const logger = spdlog::stdout_color_mt("core");
    spdlog::set_default_logger(logger);
    auto const result = run();
#ifndef NDEBUG
    core::implement::ReferenceCountedDebugger::reportLeak();
#endif
    return result;
}
