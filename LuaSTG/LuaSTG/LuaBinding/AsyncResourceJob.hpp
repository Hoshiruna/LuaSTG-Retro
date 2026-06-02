#pragma once
#include <memory>

struct lua_State;

namespace luastg {
	class AsyncResourceJob;
}

namespace luastg::binding {

	struct AsyncResourceJobBinding {
		static void registerClass(lua_State* L);
		static void createAndPush(lua_State* L, std::shared_ptr<AsyncResourceJob> job);
	};

}
