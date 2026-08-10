#include "LuaBinding/LuaWrapper.hpp"
#include "AppFrame.h"
#include "core/AudioSystem.hpp"
#include <algorithm>
#include <array>

namespace {
	constexpr char voice_metatable[] = "lstg.Audio.Voice";

<<<<<<< HEAD
	struct LuaVoice {
		core::AudioVoiceHandle handle;
	};

	core::IAudioSystem* engine(lua_State* const vm) {
		auto* const result = LAPP.getAudioSystem();
		if (result == nullptr) luaL_error(vm, "audio system is not initialized");
		return result;
	}

	LuaVoice* checkVoice(lua_State* const vm, int const index) {
		return static_cast<LuaVoice*>(luaL_checkudata(vm, index, voice_metatable));
	}

	void pushVoice(lua_State* const vm, core::AudioVoiceHandle const handle) {
		auto* const value = static_cast<LuaVoice*>(lua_newuserdata(vm, sizeof(LuaVoice)));
		value->handle = handle;
		luaL_getmetatable(vm, voice_metatable);
		lua_setmetatable(vm, -2);
	}

	void getField(lua_State* const vm, int const table, char const* const name) {
		lua_getfield(vm, table < 0 ? table - 1 : table, name);
	}

	float optionalNumberField(lua_State* const vm, int const table, char const* const name, float const fallback) {
		getField(vm, table, name);
		auto const result = lua_isnil(vm, -1) ? fallback : static_cast<float>(luaL_checknumber(vm, -1));
		lua_pop(vm, 1);
		return result;
	}

	uint64_t optionalFrameField(lua_State* const vm, int const table, char const* const name, uint64_t const fallback) {
		getField(vm, table, name);
		if (lua_isnil(vm, -1)) {
			lua_pop(vm, 1);
			return fallback;
		}
		auto const value = luaL_checknumber(vm, -1);
		if (value < 0) luaL_error(vm, "%s cannot be negative", name);
		auto const result = static_cast<uint64_t>(value);
		lua_pop(vm, 1);
		return result;
	}

	bool optionalBooleanField(lua_State* const vm, int const table, char const* const name, bool const fallback) {
		getField(vm, table, name);
		auto const result = lua_isnil(vm, -1) ? fallback : lua_toboolean(vm, -1) != 0;
		lua_pop(vm, 1);
		return result;
	}

	core::AudioPlaybackParameters readParameters(lua_State* const vm, int const index, bool const default_loop) {
		core::AudioPlaybackParameters result;
		result.loop = default_loop;
		if (lua_isnoneornil(vm, index)) return result;
		luaL_checktype(vm, index, LUA_TTABLE);
		result.volume = std::max(0.0f, optionalNumberField(vm, index, "volume", result.volume));
		result.pan = std::clamp(optionalNumberField(vm, index, "pan", result.pan), -1.0f, 1.0f);
		result.pitch = std::max(0.01f, optionalNumberField(vm, index, "pitch", result.pitch));
		result.priority = static_cast<uint8_t>(std::clamp(optionalNumberField(vm, index, "priority", result.priority), 0.0f, 255.0f));
		result.loop = optionalBooleanField(vm, index, "loop", result.loop);
		result.enable_analyzer = optionalBooleanField(vm, index, "analyzer", false);
		result.loop_range.begin = optionalFrameField(vm, index, "loop_start_frame", 0);
		result.loop_range.end = optionalFrameField(vm, index, "loop_end_frame", 0);
		return result;
	}

	core::AudioBus checkBus(lua_State* const vm, int const index) {
		auto const* const value = luaL_checkstring(vm, index);
		if (std::string_view(value) == "master") return core::AudioBus::master;
		if (std::string_view(value) == "sound_effect") return core::AudioBus::sound_effect;
		if (std::string_view(value) == "music") return core::AudioBus::music;
		luaL_error(vm, "invalid audio bus '%s'", value);
		return core::AudioBus::master;
	}

	char const* stateName(core::AudioVoiceState const state) {
		switch (state) {
		case core::AudioVoiceState::pending: return "pending";
		case core::AudioVoiceState::playing: return "playing";
		case core::AudioVoiceState::paused: return "paused";
		case core::AudioVoiceState::stopped: return "stopped";
		case core::AudioVoiceState::ended: return "ended";
		case core::AudioVoiceState::failed: return "failed";
		}
		return "failed";
	}
}

void luastg::binding::Audio::Register(lua_State* const vm) noexcept {
	struct Wrapper {
		static int ListDevices(lua_State* const vm) {
			auto* const audio = engine(vm);
			if (lua_toboolean(vm, 1) != 0) (void)audio->refreshAudioDevices();
			auto const count = audio->getAudioDeviceCount();
			lua_createtable(vm, static_cast<int>(count), 0);
			for (uint32_t i = 0; i < count; ++i) {
				auto const device = audio->getAudioDevice(i);
				lua_createtable(vm, 0, 3);
				lua_pushlstring(vm, device.id.data(), device.id.size());
				lua_setfield(vm, -2, "id");
				lua_pushlstring(vm, device.name.data(), device.name.size());
				lua_setfield(vm, -2, "name");
				lua_pushboolean(vm, device.is_default);
				lua_setfield(vm, -2, "default");
				lua_rawseti(vm, -2, static_cast<int>(i + 1));
			}
			return 1;
		}

		static int SetDevice(lua_State* const vm) {
			auto const* const id = luaL_optstring(vm, 1, "");
			lua_pushboolean(vm, engine(vm)->setAudioDevice(id));
			return 1;
		}

		static int GetCurrentDevice(lua_State* const vm) {
			auto const name = engine(vm)->getCurrentAudioDeviceName();
			lua_pushlstring(vm, name.data(), name.size());
			return 1;
		}

		static int IsSilent(lua_State* const vm) {
			lua_pushboolean(vm, engine(vm)->isSilent());
			return 1;
		}

		static int PlaySound(lua_State* const vm) {
			auto const* const name = luaL_checkstring(vm, 1);
			core::SmartReference<IResourceSoundEffect> resource = LRES.FindSound(name);
			if (!resource) return luaL_error(vm, "sound '%s' not found", name);
			auto parameters = readParameters(vm, 2, false);
			core::AudioVoiceHandle voice;
			if (!engine(vm)->play(resource->GetAudioAsset(), parameters, &voice))
				return luaL_error(vm, "failed to play sound '%s'", name);
			pushVoice(vm, voice);
			return 1;
		}

		static int PlayMusic(lua_State* const vm) {
			auto const* const name = luaL_checkstring(vm, 1);
			core::SmartReference<IResourceMusic> resource = LRES.FindMusic(name);
			if (!resource) return luaL_error(vm, "music '%s' not found", name);
			auto parameters = readParameters(vm, 2, true);
			if (parameters.loop_range.end == 0) parameters.loop_range = resource->GetLoopRange();
			core::AudioVoiceHandle voice;
			auto* const audio = engine(vm);
			if (!audio->play(resource->GetAudioAsset(), parameters, &voice))
				return luaL_error(vm, "failed to play music '%s'", name);
			if (lua_istable(vm, 2)) {
				auto const start = optionalFrameField(vm, 2, "start_frame", 0);
				if (start != 0 && !audio->seek(voice, start)) {
					audio->stop(voice);
					return luaL_error(vm, "invalid music start frame");
				}
			}
			pushVoice(vm, voice);
			return 1;
		}

		static int Pause(lua_State* const vm) {
			lua_pushboolean(vm, engine(vm)->pause(checkVoice(vm, 1)->handle));
			return 1;
		}

		static int Resume(lua_State* const vm) {
			lua_pushboolean(vm, engine(vm)->resume(checkVoice(vm, 1)->handle));
			return 1;
		}

		static int Stop(lua_State* const vm) {
			lua_pushboolean(vm, engine(vm)->stop(checkVoice(vm, 1)->handle));
			return 1;
		}

		static int Seek(lua_State* const vm) {
			auto const value = luaL_checknumber(vm, 2);
			if (value < 0) return luaL_argerror(vm, 2, "frame cannot be negative");
			auto const frame = static_cast<uint64_t>(value);
			lua_pushboolean(vm, engine(vm)->seek(checkVoice(vm, 1)->handle, frame));
			return 1;
		}

		static int SetLoopRange(lua_State* const vm) {
			auto const begin = luaL_checknumber(vm, 2);
			auto const end = luaL_checknumber(vm, 3);
			if (begin < 0 || end < 0) return luaL_error(vm, "loop frames cannot be negative");
			core::AudioFrameRange range{
				static_cast<uint64_t>(begin),
				static_cast<uint64_t>(end),
			};
			lua_pushboolean(vm, engine(vm)->setLoopRange(checkVoice(vm, 1)->handle, range));
			return 1;
		}

		static int SetLooping(lua_State* const vm) {
			lua_pushboolean(vm, engine(vm)->setLooping(checkVoice(vm, 1)->handle, lua_toboolean(vm, 2) != 0));
			return 1;
		}

		static int SetVolume(lua_State* const vm) {
			lua_pushboolean(vm, engine(vm)->setVoiceVolume(checkVoice(vm, 1)->handle, static_cast<float>(luaL_checknumber(vm, 2))));
			return 1;
		}

		static int SetPan(lua_State* const vm) {
			lua_pushboolean(vm, engine(vm)->setVoicePan(checkVoice(vm, 1)->handle, static_cast<float>(luaL_checknumber(vm, 2))));
			return 1;
		}

		static int SetPitch(lua_State* const vm) {
			lua_pushboolean(vm, engine(vm)->setVoicePitch(checkVoice(vm, 1)->handle, static_cast<float>(luaL_checknumber(vm, 2))));
			return 1;
		}

		static int GetState(lua_State* const vm) {
			lua_pushstring(vm, stateName(engine(vm)->getVoiceState(checkVoice(vm, 1)->handle)));
			return 1;
		}

		static int GetCursor(lua_State* const vm) {
			lua_pushnumber(vm, static_cast<lua_Number>(engine(vm)->getVoiceCursor(checkVoice(vm, 1)->handle)));
			return 1;
		}

		static int GetLength(lua_State* const vm) {
			lua_pushnumber(vm, static_cast<lua_Number>(engine(vm)->getVoiceLength(checkVoice(vm, 1)->handle)));
			return 1;
		}

		static int GetSpectrum(lua_State* const vm) {
			std::array<float, 256> spectrum{};
			auto const count = engine(vm)->getSpectrum(checkVoice(vm, 1)->handle, spectrum.data(), static_cast<uint32_t>(spectrum.size()));
			if (lua_istable(vm, 2)) lua_pushvalue(vm, 2);
			else lua_createtable(vm, static_cast<int>(count), 0);
			for (uint32_t i = 0; i < count; ++i) {
				lua_pushnumber(vm, spectrum[i]);
				lua_rawseti(vm, -2, static_cast<int>(i + 1));
			}
			return 1;
		}

		static int SetBusVolume(lua_State* const vm) {
			engine(vm)->setBusVolume(checkBus(vm, 1), static_cast<float>(luaL_checknumber(vm, 2)));
			return 0;
		}

		static int GetBusVolume(lua_State* const vm) {
			lua_pushnumber(vm, engine(vm)->getBusVolume(checkBus(vm, 1)));
			return 1;
		}

		static int SetMaxVoices(lua_State* const vm) {
			auto const value = luaL_checkinteger(vm, 1);
			if (value < 1 || value > 256) return luaL_argerror(vm, 1, "expected a value in [1, 256]");
			engine(vm)->setMaxSoundEffectVoices(static_cast<uint32_t>(value));
			return 0;
		}

		static int GetMaxVoices(lua_State* const vm) {
			lua_pushinteger(vm, engine(vm)->getMaxSoundEffectVoices());
			return 1;
		}

		static int VoiceToString(lua_State* const vm) {
			auto const handle = checkVoice(vm, 1)->handle;
			lua_pushfstring(vm, "AudioVoice(%d:%d)", static_cast<int>(handle.index), static_cast<int>(handle.generation));
			return 1;
		}
	};

	if (luaL_newmetatable(vm, voice_metatable)) {
		lua_pushcfunction(vm, Wrapper::VoiceToString);
		lua_setfield(vm, -2, "__tostring");
	}
	lua_pop(vm, 1);

	luaL_Reg const library[] = {
		{ "listDevices", Wrapper::ListDevices },
		{ "setDevice", Wrapper::SetDevice },
		{ "getCurrentDevice", Wrapper::GetCurrentDevice },
		{ "isSilent", Wrapper::IsSilent },
		{ "playSound", Wrapper::PlaySound },
		{ "playMusic", Wrapper::PlayMusic },
		{ "pause", Wrapper::Pause },
		{ "resume", Wrapper::Resume },
		{ "stop", Wrapper::Stop },
		{ "seek", Wrapper::Seek },
		{ "setLoopRange", Wrapper::SetLoopRange },
		{ "setLooping", Wrapper::SetLooping },
		{ "setVolume", Wrapper::SetVolume },
		{ "setPan", Wrapper::SetPan },
		{ "setPitch", Wrapper::SetPitch },
		{ "getState", Wrapper::GetState },
		{ "getCursor", Wrapper::GetCursor },
		{ "getLength", Wrapper::GetLength },
		{ "getSpectrum", Wrapper::GetSpectrum },
		{ "setBusVolume", Wrapper::SetBusVolume },
		{ "getBusVolume", Wrapper::GetBusVolume },
		{ "setMaxVoices", Wrapper::SetMaxVoices },
		{ "getMaxVoices", Wrapper::GetMaxVoices },
		{ nullptr, nullptr },
	};

	luaL_register(vm, LUASTG_LUA_LIBNAME ".Audio", library);
	lua_pop(vm, 1);
=======
void
luastg::binding::Audio::Register(lua_State* L) noexcept
{
    struct Wrapper
    {
        static int ListAudioDevice(lua_State* vm)
        {
            lua::stack_t const ctx(vm);
            auto const audio_engine = LAPP.getAudioEngine();
            if(!audio_engine) {
                return luaL_error(vm, "engine not initialized");
            }
            if(auto const refresh = ctx.get_value<bool>(1); refresh) {
                std::ignore = audio_engine->refreshAudioEndpoint();
            }
            uint32_t const count = audio_engine->getAudioEndpointCount();
            auto const names = ctx.create_array(count);
            for(uint32_t i = 0; i < count; i += 1) {
                auto const name = audio_engine->getAudioEndpointName(i);
                ctx.set_array_value_zero_base<std::string_view>(names, i, name);
            }
            return 1;
        }
        static int ChangeAudioDevice(lua_State* L)
        {
            auto const audio_engine = LAPP.getAudioEngine();
            if(!audio_engine) {
                return luaL_error(L, "engine not initialized");
            }
            lua::stack_t S(L);
            auto const name = S.get_value<std::string_view>(1);
            auto const result = audio_engine->setAudioEndpoint(name);
            S.push_value<bool>(result);
            return 1;
        }
        static int GetCurrentAudioDeviceName(lua_State* L)
        {
            auto const audio_engine = LAPP.getAudioEngine();
            if(!audio_engine) {
                return luaL_error(L, "engine not initialized");
            }
            lua::stack_t S(L);
            auto const result = audio_engine->getCurrentAudioEndpointName();
            S.push_value<std::string_view>(result);
            return 1;
        }

        static int PlaySound(lua_State* vm) noexcept
        {
            lua::stack_t const ctx(vm);
            auto const name = ctx.get_value<std::string_view>(1);
            auto const volume = ctx.get_value<float>(2, 1.0f);
            auto const pan = ctx.get_value<float>(3, 0.0f);
            core::SmartReference<IResourceSoundEffect> p = LRES.FindSound(name.data());
            if(!p)
                return luaL_error(vm, "sound '%s' not found.", name.data());
            p->Play(std::clamp(volume, 0.0f, 1.0f), std::clamp(pan, -1.0f, 1.0f));
            return 0;
        }
        static int StopSound(lua_State* L) noexcept
        {
            const char* s = luaL_checkstring(L, 1);
            core::SmartReference<IResourceSoundEffect> p = LRES.FindSound(s);
            if(!p)
                return luaL_error(L, "sound '%s' not found.", s);
            p->Stop();
            return 0;
        }
        static int PauseSound(lua_State* L) noexcept
        {
            const char* s = luaL_checkstring(L, 1);
            core::SmartReference<IResourceSoundEffect> p = LRES.FindSound(s);
            if(!p)
                return luaL_error(L, "sound '%s' not found.", s);
            p->Pause();
            return 0;
        }
        static int ResumeSound(lua_State* L) noexcept
        {
            const char* s = luaL_checkstring(L, 1);
            core::SmartReference<IResourceSoundEffect> p = LRES.FindSound(s);
            if(!p)
                return luaL_error(L, "sound '%s' not found.", s);
            p->Resume();
            return 0;
        }
        static int GetSoundState(lua_State* L) noexcept
        {
            const char* s = luaL_checkstring(L, 1);
            core::SmartReference<IResourceSoundEffect> p = LRES.FindSound(s);
            if(!p)
                return luaL_error(L, "sound '%s' not found.", s);
            if(p->IsPlaying())
                lua_pushstring(L, "playing");
            else if(p->IsStopped())
                lua_pushstring(L, "stopped");
            else
                lua_pushstring(L, "paused");
            return 1;
        }
        static int SetSEVolume(lua_State* vm) noexcept
        {
            lua::stack_t const ctx(vm);
            auto const volume = ctx.get_value<float>(1);
            LAPP.SetSEVolume(std::clamp(volume, 0.0f, 1.0f));
            return 0;
        }
        static int GetSEVolume(lua_State* L)
        {
            lua_pushnumber(L, LAPP.GetSEVolume());
            return 1;
        }
        static int SetSESpeed(lua_State* L)
        {
            const char* s = luaL_checkstring(L, 1);
            float speed = (float)luaL_checknumber(L, 2);
            core::SmartReference<IResourceSoundEffect> p = LRES.FindSound(s);
            if(!p)
                return luaL_error(L, "sound '%s' not found.", s);
            if(!p->SetSpeed(speed))
                return luaL_error(L, "Can't set sound('%s') playing speed.", s);
            return 0;
        }
        static int GetSESpeed(lua_State* L)
        {
            const char* s = luaL_checkstring(L, 1);
            core::SmartReference<IResourceSoundEffect> p = LRES.FindSound(s);
            if(!p)
                return luaL_error(L, "sound '%s' not found.", s);
            lua_pushnumber(L, p->GetSpeed());
            return 1;
        }
        static int UpdateSound(lua_State*) noexcept
        {
            // 否决的方法
            return 0;
        }

        static int PlayMusic(lua_State* vm) noexcept
        {
            lua::stack_t const ctx(vm);
            auto const name = ctx.get_value<std::string_view>(1);
            auto const volume = ctx.get_value<float>(2, 1.0f);
            auto const position = ctx.get_value<double>(3, 0.0f);
            core::SmartReference<IResourceMusic> p = LRES.FindMusic(name.data());
            if(!p)
                return luaL_error(vm, "music '%s' not found.", name.data());
            p->Play(std::clamp(volume, 0.0f, 1.0f), position);
            return 0;
        }
        static int StopMusic(lua_State* L) noexcept
        {
            const char* s = luaL_checkstring(L, 1);
            core::SmartReference<IResourceMusic> p = LRES.FindMusic(s);
            if(!p)
                return luaL_error(L, "music '%s' not found.", s);
            p->Stop();
            return 0;
        }
        static int PauseMusic(lua_State* L) noexcept
        {
            const char* s = luaL_checkstring(L, 1);
            core::SmartReference<IResourceMusic> p = LRES.FindMusic(s);
            if(!p)
                return luaL_error(L, "music '%s' not found.", s);
            p->Pause();
            return 0;
        }
        static int ResumeMusic(lua_State* L) noexcept
        {
            const char* s = luaL_checkstring(L, 1);
            core::SmartReference<IResourceMusic> p = LRES.FindMusic(s);
            if(!p)
                return luaL_error(L, "music '%s' not found.", s);
            p->Resume();
            return 0;
        }
        static int GetMusicState(lua_State* L) noexcept
        {
            const char* s = luaL_checkstring(L, 1);
            core::SmartReference<IResourceMusic> p = LRES.FindMusic(s);
            if(!p)
                return luaL_error(L, "music '%s' not found.", s);
            if(p->IsPlaying())
                lua_pushstring(L, "playing");
            else if(p->IsPaused())
                lua_pushstring(L, "paused");
            //else if (p->IsStopped())
            //lua_pushstring(L, "stopped");
            else
                lua_pushstring(L, "stopped");
            //lua_pushstring(L, "paused");
            return 1;
        }
        static int GetMusicFFT(lua_State* L) noexcept
        {
            const char* s = luaL_checkstring(L, 1);
            core::SmartReference<IResourceMusic> p = LRES.FindMusic(s);
            if(!p)
                return luaL_error(L, "music '%s' not found.", s);
            p->GetAudioPlayer()->updateFFT();
            size_t sz = p->GetAudioPlayer()->getFFTSize();
            float const* fdata = p->GetAudioPlayer()->getFFT();
            if(!lua_istable(L, 2)) {
                lua_createtable(L, (int)sz, 0);
            }
            for(int i = 0; i < (int)sz; i += 1) {
                lua_pushnumber(L, (lua_Number)fdata[i]);
                lua_rawseti(L, 2, i + 1);
            }
            return 1;
        }
        static int SetMusicLoopRange(lua_State* L)
        {
            lua::stack_t S(L);

            auto const music_name = S.get_value<std::string_view>(1);
            core::SmartReference<IResourceMusic> p = LRES.FindMusic(music_name.data());
            if(!p)
                return luaL_error(L, "music '%s' not found.", music_name.data());

            // 没有第二个参数，禁用循环

            if(S.index_of_top().value <= 1 || S.is_nil(2)) {
                MusicRoopRange range{};
                range.type = MusicRoopRangeType::Disable;
                p->SetLoopRange(range);
                return 0;
            }

            lua::stack_index_t a_range(2);
            MusicRoopRange range{};
            if(!S.is_table(a_range))
                return luaL_error(L, "invalid parameter #2, required table");

            // 按采样

            if(S.has_map_value(a_range, "start_in_samples") && S.has_map_value(a_range, "end_in_samples")) {
                range.type = MusicRoopRangeType::StartPointAndEndPoint;
                range.unit = MusicRoopRangeUnit::Sample;
                range.start_in_samples = S.get_map_value<uint32_t>(a_range, "start_in_samples");
                range.end_in_samples = S.get_map_value<uint32_t>(a_range, "end_in_samples");
            } else if(S.has_map_value(a_range, "start_in_samples") && S.has_map_value(a_range, "length_in_samples")) {
                range.type = MusicRoopRangeType::StartPointAndLength;
                range.unit = MusicRoopRangeUnit::Sample;
                range.start_in_samples = S.get_map_value<uint32_t>(a_range, "start_in_samples");
                range.length_in_samples = S.get_map_value<uint32_t>(a_range, "length_in_samples");
            } else if(S.has_map_value(a_range, "length_in_samples") && S.has_map_value(a_range, "end_in_samples")) {
                range.type = MusicRoopRangeType::LengthAndEndPoint;
                range.unit = MusicRoopRangeUnit::Sample;
                range.length_in_samples = S.get_map_value<uint32_t>(a_range, "length_in_samples");
                range.end_in_samples = S.get_map_value<uint32_t>(a_range, "end_in_samples");
            } else if(S.has_map_value(a_range, "start_in_samples")) {
                range.type = MusicRoopRangeType::StartPointToEnd;
                range.unit = MusicRoopRangeUnit::Sample;
                range.start_in_samples = S.get_map_value<uint32_t>(a_range, "start_in_samples");
            } else if(S.has_map_value(a_range, "end_in_samples")) {
                range.type = MusicRoopRangeType::StartToEndPoint;
                range.unit = MusicRoopRangeUnit::Sample;
                range.end_in_samples = S.get_map_value<uint32_t>(a_range, "end_in_samples");
            }

            // 按秒

            else if(S.has_map_value(a_range, "start_in_seconds") && S.has_map_value(a_range, "end_in_seconds")) {
                range.type = MusicRoopRangeType::StartPointAndEndPoint;
                range.unit = MusicRoopRangeUnit::Second;
                range.start_in_seconds = S.get_map_value<double>(a_range, "start_in_seconds");
                range.end_in_seconds = S.get_map_value<double>(a_range, "end_in_seconds");
            } else if(S.has_map_value(a_range, "start_in_seconds") && S.has_map_value(a_range, "length_in_seconds")) {
                range.type = MusicRoopRangeType::StartPointAndLength;
                range.unit = MusicRoopRangeUnit::Second;
                range.start_in_seconds = S.get_map_value<double>(a_range, "start_in_seconds");
                range.length_in_seconds = S.get_map_value<double>(a_range, "length_in_seconds");
            } else if(S.has_map_value(a_range, "length_in_seconds") && S.has_map_value(a_range, "end_in_seconds")) {
                range.type = MusicRoopRangeType::LengthAndEndPoint;
                range.unit = MusicRoopRangeUnit::Second;
                range.length_in_seconds = S.get_map_value<double>(a_range, "length_in_seconds");
                range.end_in_seconds = S.get_map_value<double>(a_range, "end_in_seconds");
            } else if(S.has_map_value(a_range, "start_in_seconds")) {
                range.type = MusicRoopRangeType::StartPointToEnd;
                range.unit = MusicRoopRangeUnit::Second;
                range.start_in_seconds = S.get_map_value<double>(a_range, "start_in_seconds");
            } else if(S.has_map_value(a_range, "end_in_seconds")) {
                range.type = MusicRoopRangeType::StartToEndPoint;
                range.unit = MusicRoopRangeUnit::Second;
                range.end_in_seconds = S.get_map_value<double>(a_range, "end_in_seconds");
            }

            // 全曲循环

            else {
                range.type = MusicRoopRangeType::All;
            }

            p->SetLoopRange(range);
            return 0;
        }
        static int SetBGMVolume(lua_State* vm) noexcept
        {
            lua::stack_t const ctx(vm);
            if(lua_gettop(vm) <= 1) {
                auto const volume = ctx.get_value<float>(1);
                LAPP.SetBGMVolume(std::clamp(volume, 0.0f, 1.0f));
            } else {
                auto const name = ctx.get_value<std::string_view>(1);
                auto const volume = ctx.get_value<float>(2);
                core::SmartReference<IResourceMusic> p = LRES.FindMusic(name.data());
                if(!p)
                    return luaL_error(vm, "music '%s' not found.", name.data());
                p->SetVolume(std::clamp(volume, 0.0f, 1.0f));
            }
            return 0;
        }
        static int GetBGMVolume(lua_State* L) noexcept
        {
            if(lua_gettop(L) == 0) {
                lua_pushnumber(L, LAPP.GetBGMVolume());
            } else if(lua_gettop(L) == 1) {
                const char* s = luaL_checkstring(L, 1);
                core::SmartReference<IResourceMusic> p = LRES.FindMusic(s);
                if(!p)
                    return luaL_error(L, "music '%s' not found.", s);
                lua_pushnumber(L, p->GetVolume());
            }
            return 1;
        }
        static int SetBGMSpeed(lua_State* L)
        {
            const char* s = luaL_checkstring(L, 1);
            float speed = (float)luaL_checknumber(L, 2);
            core::SmartReference<IResourceMusic> p = LRES.FindMusic(s);
            if(!p)
                return luaL_error(L, "music '%s' not found.", s);
            if(!p->SetSpeed(speed))
                return luaL_error(L, "Can't set music('%s') playing speed.", s);
            return 0;
        }
        static int GetBGMSpeed(lua_State* L)
        {
            const char* s = luaL_checkstring(L, 1);
            core::SmartReference<IResourceMusic> p = LRES.FindMusic(s);
            if(!p)
                return luaL_error(L, "music '%s' not found.", s);
            lua_pushnumber(L, p->GetSpeed());
            return 1;
        }
        static int SetBGMLoop(lua_State* L)
        {
            const char* s = luaL_checkstring(L, 1);
            bool loop = lua_toboolean(L, 2);
            core::SmartReference<IResourceMusic> p = LRES.FindMusic(s);
            if(!p)
                return luaL_error(L, "music '%s' not found.", s);
            p->SetLoop(loop);
            return 0;
        }
    };

    luaL_Reg const lib[] = {
        { "ListAudioDevice", &Wrapper::ListAudioDevice },
        { "ChangeAudioDevice", &Wrapper::ChangeAudioDevice },
        { "GetCurrentAudioDeviceName", &Wrapper::GetCurrentAudioDeviceName },

        { "PlaySound", &Wrapper::PlaySound },
        { "StopSound", &Wrapper::StopSound },
        { "PauseSound", &Wrapper::PauseSound },
        { "ResumeSound", &Wrapper::ResumeSound },
        { "GetSoundState", &Wrapper::GetSoundState },
        { "SetSEVolume", &Wrapper::SetSEVolume },
        { "GetSEVolume", &Wrapper::GetSEVolume },
        { "SetSESpeed", &Wrapper::SetSESpeed },
        { "GetSESpeed", &Wrapper::GetSESpeed },
        { "UpdateSound", &Wrapper::UpdateSound },

        { "PlayMusic", &Wrapper::PlayMusic },
        { "StopMusic", &Wrapper::StopMusic },
        { "PauseMusic", &Wrapper::PauseMusic },
        { "ResumeMusic", &Wrapper::ResumeMusic },
        { "GetMusicState", &Wrapper::GetMusicState },
        { "GetMusicFFT", &Wrapper::GetMusicFFT },
        { "SetMusicLoopRange", &Wrapper::SetMusicLoopRange },
        { "SetBGMVolume", &Wrapper::SetBGMVolume },
        { "GetBGMVolume", &Wrapper::GetBGMVolume },
        { "SetBGMSpeed", &Wrapper::SetBGMSpeed },
        { "GetBGMSpeed", &Wrapper::GetBGMSpeed },
        { "SetBGMLoop", &Wrapper::SetBGMLoop },
        { NULL, NULL },
    };

    luaL_Reg const lib_empty[] = {
        { NULL, NULL },
    };

    luaL_register(L, LUASTG_LUA_LIBNAME, lib); // ??? lstg
    luaL_register(L, LUASTG_LUA_LIBNAME ".Audio", lib); // ??? lstg lstg.Audio
    lua_setfield(L, -1, "Audio"); // ??? lstg
    lua_pop(L, 1); // ???
>>>>>>> origin/master
}
