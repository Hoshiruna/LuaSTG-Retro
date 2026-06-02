#include "LuaBinding/AsyncResourceJob.hpp"
#include "GameResource/AsyncResourceLoader.hpp"
#include "lua/plus.hpp"
#include "lua.hpp"

#include <optional>
#include <string_view>
#include <utility>

using std::string_view_literals::operator ""sv;

namespace luastg::binding {
	namespace {
		constexpr std::string_view class_name{ "lstg.AsyncResourceJob"sv };

		struct JobUserData {
			std::shared_ptr<AsyncResourceJob>* job{};
		};

		JobUserData* cast(lua_State* const L, int const index) {
			return static_cast<JobUserData*>(luaL_checkudata(L, index, class_name.data()));
		}

		static int gc(lua_State* const L) {
			auto* self = cast(L, 1);
			delete self->job;
			self->job = nullptr;
			return 0;
		}

		static int tostring(lua_State* const L) {
			lua::stack_t const S(L);
			S.push_value(class_name);
			return 1;
		}

		static int status(lua_State* const L) {
			lua::stack_t const S(L);
			auto* self = cast(L, 1);
			if (!self->job || !*self->job) {
				S.push_value("cancelled"sv);
				return 1;
			}
			S.push_value((*self->job)->getStateName());
			return 1;
		}

		static int isDone(lua_State* const L) {
			lua::stack_t const S(L);
			auto* self = cast(L, 1);
			S.push_value(self->job && *self->job && (*self->job)->isFinished());
			return 1;
		}

		static int error(lua_State* const L) {
			lua::stack_t const S(L);
			auto* self = cast(L, 1);
			if (!self->job || !*self->job) {
				S.push_value(std::nullopt);
				return 1;
			}
			auto message = (*self->job)->getError();
			if (message.empty()) {
				S.push_value(std::nullopt);
			}
			else {
				S.push_value(message);
			}
			return 1;
		}

		static int read(lua_State* const L) {
			lua::stack_t const S(L);
			auto* self = cast(L, 1);
			if (!self->job || !*self->job) {
				S.push_value(std::nullopt);
				S.push_value("cancelled"sv);
				return 2;
			}

			auto const state = (*self->job)->getState();
			if (state == AsyncResourceJobState::Queued
				|| state == AsyncResourceJobState::Running
				|| state == AsyncResourceJobState::Ready) {
				S.push_value(std::nullopt);
				S.push_value("pending"sv);
				return 2;
			}
			if (state == AsyncResourceJobState::Cancelled) {
				S.push_value(std::nullopt);
				S.push_value("cancelled"sv);
				return 2;
			}
			if (state == AsyncResourceJobState::Failed) {
				auto message = (*self->job)->getError();
				std::string_view const view = message.empty() ? "failed"sv : std::string_view(message);
				S.push_value(std::nullopt);
				S.push_value(view);
				return 2;
			}

			if ((*self->job)->getKind() == AsyncResourceJobKind::FileRead) {
				auto data = (*self->job)->getFileData();
				if (!data) {
					S.push_value(std::nullopt);
					S.push_value("failed"sv);
					return 2;
				}
				lua_pushlstring(L, static_cast<char const*>(data->data()), data->size());
				return 1;
			}

			S.push_value(true);
			return 1;
		}

		static int cancel(lua_State* const L) {
			lua::stack_t const S(L);
			auto* self = cast(L, 1);
			S.push_value(self->job && *self->job && (*self->job)->cancel());
			return 1;
		}
	}

	void AsyncResourceJobBinding::registerClass(lua_State* const L) {
		lua::stack_balancer_t const sb(L);
		lua::stack_t const S(L);

		auto const method_table = S.create_map();
		S.set_map_value(method_table, "status"sv, &status);
		S.set_map_value(method_table, "isDone"sv, &isDone);
		S.set_map_value(method_table, "error"sv, &error);
		S.set_map_value(method_table, "read"sv, &read);
		S.set_map_value(method_table, "cancel"sv, &cancel);

		auto const metatable = S.create_metatable(class_name);
		S.set_map_value(metatable, "__gc"sv, &gc);
		S.set_map_value(metatable, "__tostring"sv, &tostring);
		S.set_map_value(metatable, "__index"sv, method_table);
	}

	void AsyncResourceJobBinding::createAndPush(lua_State* const L, std::shared_ptr<AsyncResourceJob> job) {
		lua::stack_t const S(L);
		auto* self = S.create_userdata<JobUserData>();
		auto const self_index = S.index_of_top();
		S.set_metatable(self_index, class_name);
		self->job = new std::shared_ptr<AsyncResourceJob>(std::move(job));
	}

}
