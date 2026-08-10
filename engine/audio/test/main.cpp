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
}
void
setupLogger()
{
    auto const logger = spdlog::stdout_color_mt("core");
    spdlog::set_default_logger(logger);
    win32::set_logger_writer([](std::string_view const message) -> void {
        core::Logger::error(message);
    });
}

core::SmartReference<core::IAudioEngine> engine;
core::SmartReference<core::IAudioDecoder> decoder;
core::SmartReference<core::IAudioPlayer> player;

void
commandEndpointCurrent()
{
    std::println("current endpoint: {}", engine->getCurrentAudioEndpointName());
}
void
commandEndpointList()
{
    if(!engine->refreshAudioEndpoint()) {
        core::Logger::error("core::IAudioEngine::refreshAudioEndpoint");
        return;
    }
    for(uint32_t i = 0; i < engine->getAudioEndpointCount(); i++) {
        std::println("{}. {}", i, engine->getAudioEndpointName(i));
    }
}
void
commandEndpointSet(uint32_t const index)
{
    if(index >= engine->getAudioEndpointCount()) {
        core::Logger::error("endpoint {} not exists", index);
        return;
    }
    if(!engine->setAudioEndpoint(engine->getAudioEndpointName(index))) {
        core::Logger::error("core::IAudioEngine::setAudioEndpoint");
    }
}
void
commandOpen(std::string const& arg)
{
    auto const begin = arg.find_first_of('"');
    auto const end = arg.find_last_of('"');
    if(begin == end) {
        core::Logger::error("path is empty");
        return;
    }
    if(begin == std::string::npos || end == std::string::npos) {
        core::Logger::error("invalid path");
        return;
    }
    auto const path = std::string_view(arg).substr(begin + 1, end - begin - 1);

    if(!core::FileSystemManager::hasFile(path)) {
        core::Logger::error("file '{}' not exists", path);
        return;
    }

    if(!core::IAudioDecoder::create(path, decoder.put())) {
        core::Logger::error("core::IAudioDecoder::create");
        return;
    }

    if(!engine->createStreamAudioPlayer(decoder.get(), core::AudioMixingChannel::music, player.put())) {
        core::Logger::error("core::IAudioEngine::createAudioPlayer");
        return;
    }

    std::println("'{}' opened", path);
}
void
commandClose()
{
    player.reset();
    decoder.reset();
}
void
commandPlay()
{
    if(!player) {
        core::Logger::error("encoder and player not initialized, please open a file");
        return;
    }
    player->play(0.0);
}
void
commandPause()
{
    if(!player) {
        core::Logger::error("encoder and player not initialized, please open a file");
        return;
    }
    player->pause();
}
void
commandResume()
{
    if(!player) {
        core::Logger::error("encoder and player not initialized, please open a file");
        return;
    }
    player->resume();
}
void
commandStop()
{
    if(!player) {
        core::Logger::error("encoder and player not initialized, please open a file");
        return;
    }
    player->stop();
}
void
commandLoopSet(std::string_view const type, double const start, double const length)
{
    if(!player) {
        core::Logger::error("encoder and player not initialized, please open a file");
        return;
    }
    if(type == "off"sv) {
        player->setLoop(false, 0.0, 0.0);
    } else if(type == "all"sv) {
        player->setLoop(true, 0, static_cast<double>(decoder->getFrameCount()) / static_cast<double>(decoder->getSampleRate()));
    } else if(type == "range"sv) {
        player->setLoop(true, start, length);
    }
}
void
destroyResources()
{
    engine.reset();
    player.reset();
    decoder.reset();
}
int
run()
{
    if(!core::IAudioEngine::create(engine.put())) {
        core::Logger::error("core::IAudioEngine::create");
        return 1;
    }

    std::string command;
    while(true) {
        std::print("> "sv);
        std::cin >> command;
        if(command == "exit"sv) {
            break;
        }
        if(command == "endpoint"sv) {
            std::cin >> command;
            if(command == "current"sv) {
                commandEndpointCurrent();
            } else if(command == "list"sv) {
                commandEndpointList();
            } else if(command == "set"sv) {
                int index{};
                std::cin >> index;
                if(index < 0) {
                    core::Logger::error("endpoint {} not exists", index);
                } else {
                    commandEndpointSet(static_cast<uint32_t>(index));
                }
            } else {
                printHelp();
            }
        } else if(command == "open"sv) {
            std::getline(std::cin, command);
            commandOpen(command);
        } else if(command == "close"sv) {
            commandClose();
        } else if(command == "play"sv) {
            commandPlay();
        } else if(command == "pause"sv) {
            commandPause();
        } else if(command == "resume"sv) {
            commandResume();
        } else if(command == "stop"sv) {
            commandStop();
        } else if(command == "loop"sv) {
            std::cin >> command;
            if(command == "off"sv) {
                commandLoopSet("off"sv, 0.0, 0.0);
            } else if(command == "all"sv) {
                commandLoopSet("all"sv, 0.0, 0.0);
            } else if(command == "range"sv) {
                double start{}, length{};
                std::cin >> start >> length;
                commandLoopSet("range"sv, start, length);
            } else {
                printHelp();
            }
        } else {
            printHelp();
        }
    }

    destroyResources();
    return 0;
}
}

int
main()
{
    setupEnvironment();
    setupLogger();
    auto const result = run();
#ifndef NDEBUG
    core::implement::ReferenceCountedDebugger::reportLeak();
#endif
    return result;
}
