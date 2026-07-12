#include "lua/plus.hpp"
#include "AppFrame.h"
#include "LuaBinding/Resource.hpp"

namespace luastg::binding
{
	static constexpr std::string_view const ModuleID{ "LuaSTG.Sub" };

	struct ResourceTexture
	{
		static constexpr std::string_view const ClassID{ "LuaSTG.Sub.ResourceTexture" };

		luastg::IResourceTexture* data;

		static int api_getResourceType(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			S.push_value(static_cast<int32_t>(self->data->GetType()));
			return 1;
		}
		static int api_getResourceName(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			S.push_value(self->data->GetResName());
			return 1;
		}
		static int api_getSize(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			auto const result = self->data->GetTexture()->getSize();
			S.push_value(result.x);
			S.push_value(result.y);
			return 2;
		}
		static int api_getWidth(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			auto const result = self->data->GetTexture()->getSize();
			S.push_value(result.x);
			return 1;
		}
		static int api_getHeight(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			auto const result = self->data->GetTexture()->getSize();
			S.push_value(result.y);
			return 1;
		}

		static int api___gc(lua_State* L)
		{
			auto* self = cast(L, 1);
			if (self->data)
			{
				self->data->release();
				self->data = nullptr;
			}
			return 0;
		}
		static int api___tostring(lua_State* L)
		{
			lua::stack_t S(L);
			std::ignore = cast(L, 1);
			S.push_value<std::string_view>(ClassID);
			return 1;
		}
		static int api___eq(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			if (test(L, 2)) {
				auto* other = cast(L, 2);
				S.push_value(self->data == other->data);
			}
			else {
				S.push_value(false);
			}
			return 1;
		}

		static ResourceTexture* create(lua_State* L)
		{
			lua::stack_t S(L);

			auto* self = S.create_userdata<ResourceTexture>();
			auto const self_index = S.index_of_top();
			S.set_metatable(self_index, ClassID);

			self->data = nullptr;
			return self;
		}
		static ResourceTexture* cast(lua_State* L, int idx)
		{
			return static_cast<ResourceTexture*>(luaL_checkudata(L, idx, ClassID.data()));
		}
		static bool test(lua_State* L, int idx)
		{
			return nullptr != luaL_testudata(L, idx, ClassID.data());
		}
		static void registerClass(lua_State* L)
		{
			[[maybe_unused]] lua::stack_balancer_t SB(L);
			lua::stack_t S(L);

			// method

			auto const method_table = S.create_map();
			S.set_map_value(method_table, "getResourceType", &api_getResourceType);
			S.set_map_value(method_table, "getResourceName", &api_getResourceName);
			//S.set_map_value(method_table, "getSize", &api_getSize);
			S.set_map_value(method_table, "getWidth", &api_getWidth);
			S.set_map_value(method_table, "getHeight", &api_getHeight);

			// metatable

			auto const metatable = S.create_metatable(ClassID);
			S.set_map_value(metatable, "__gc", &api___gc);
			S.set_map_value(metatable, "__tostring", &api___tostring);
			S.set_map_value(metatable, "__eq", &api___eq);
			S.set_map_value(metatable, "__index", method_table);

			// factory

			// 暂时不暴露出创建接口
			//auto const class_table = S.create_map();
			//S.set_map_value(class_table, "createFromFile", &api_createFromFile);

			// register

			// 暂时不暴露出创建接口
			//auto const M = S.push_module("LuaSTG.Sub");
			//S.set_map_value(M, "Texture2D", class_table);
		}
	};

	struct ResourceSprite
	{
		static constexpr std::string_view const ClassID{ "LuaSTG.Sub.ResourceSprite" };

		luastg::IResourceSprite* data;

		// IResource
		static int api_getResourceType(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			S.push_value(static_cast<int32_t>(self->data->GetType()));
			return 1;
		}
		static int api_getResourceName(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			S.push_value(self->data->GetResName());
			return 1;
		}
		// IResourceSprite
		static int api_setCenter(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			auto const x = S.get_value<float>(2);
			auto const y = S.get_value<float>(3);
			self->data->GetSprite()->setTextureCenter(core::Vector2F(x, y));
			return 0;
		}
		static int api_setUnitsPerPixel(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			auto const v = S.get_value<float>(2);
			self->data->GetSprite()->setUnitsPerPixel(v);
			return 0;
		}

		static int api___gc(lua_State* L)
		{
			auto* self = cast(L, 1);
			if (self->data)
			{
				self->data->release();
				self->data = nullptr;
			}
			return 0;
		}
		static int api___tostring(lua_State* L)
		{
			lua::stack_t S(L);
			std::ignore = cast(L, 1);
			S.push_value<std::string_view>(ClassID);
			return 1;
		}
		static int api___eq(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			if (test(L, 2)) {
				auto* other = cast(L, 2);
				S.push_value(self->data == other->data);
			}
			else {
				S.push_value(false);
			}
			return 1;
		}

		static ResourceSprite* create(lua_State* L)
		{
			lua::stack_t S(L);

			auto* self = S.create_userdata<ResourceSprite>();
			auto const self_index = S.index_of_top();
			S.set_metatable(self_index, ClassID);

			self->data = nullptr;
			return self;
		}
		static ResourceSprite* cast(lua_State* L, int idx)
		{
			return static_cast<ResourceSprite*>(luaL_checkudata(L, idx, ClassID.data()));
		}
		static bool test(lua_State* L, int idx)
		{
			return nullptr != luaL_testudata(L, idx, ClassID.data());
		}
		static void registerClass(lua_State* L)
		{
			[[maybe_unused]] lua::stack_balancer_t SB(L);
			lua::stack_t S(L);

			// method

			auto const method_table = S.create_map();
			S.set_map_value(method_table, "getResourceType", &api_getResourceType);
			S.set_map_value(method_table, "getResourceName", &api_getResourceName);
			S.set_map_value(method_table, "setCenter", &api_setCenter);
			S.set_map_value(method_table, "setUnitsPerPixel", &api_setUnitsPerPixel);

			// metatable

			auto const metatable = S.create_metatable(ClassID);
			S.set_map_value(metatable, "__gc", &api___gc);
			S.set_map_value(metatable, "__tostring", &api___tostring);
			S.set_map_value(metatable, "__eq", &api___eq);
			S.set_map_value(metatable, "__index", method_table);

			// factory

			// 暂时不暴露出创建接口
			//auto const class_table = S.create_map();
			//S.set_map_value(class_table, "createFromFile", &api_createFromFile);

			// register

			// 暂时不暴露出创建接口
			//auto const M = S.push_module("LuaSTG.Sub");
			//S.set_map_value(M, "Texture2D", class_table);
		}
	};
	
	struct ResourceSpriteSequence
	{
		static constexpr std::string_view const ClassID{ "LuaSTG.Sub.ResourceSpriteSequence" };

		luastg::IResourceAnimation* data;

		static int api_getResourceType(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			S.push_value(static_cast<int32_t>(self->data->GetType()));
			return 1;
		}
		static int api_getResourceName(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			S.push_value(self->data->GetResName());
			return 1;
		}

		static int api___gc(lua_State* L)
		{
			auto* self = cast(L, 1);
			if (self->data)
			{
				self->data->release();
				self->data = nullptr;
			}
			return 0;
		}
		static int api___tostring(lua_State* L)
		{
			lua::stack_t S(L);
			std::ignore = cast(L, 1);
			S.push_value<std::string_view>(ClassID);
			return 1;
		}
		static int api___eq(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			if (test(L, 2)) {
				auto* other = cast(L, 2);
				S.push_value(self->data == other->data);
			}
			else {
				S.push_value(false);
			}
			return 1;
		}

		static ResourceSpriteSequence* create(lua_State* L)
		{
			lua::stack_t S(L);

			auto* self = S.create_userdata<ResourceSpriteSequence>();
			auto const self_index = S.index_of_top();
			S.set_metatable(self_index, ClassID);

			self->data = nullptr;
			return self;
		}
		static ResourceSpriteSequence* cast(lua_State* L, int idx)
		{
			return static_cast<ResourceSpriteSequence*>(luaL_checkudata(L, idx, ClassID.data()));
		}
		static bool test(lua_State* L, int idx)
		{
			return nullptr != luaL_testudata(L, idx, ClassID.data());
		}
		static void registerClass(lua_State* L)
		{
			[[maybe_unused]] lua::stack_balancer_t SB(L);
			lua::stack_t S(L);

			// method

			auto const method_table = S.create_map();
			S.set_map_value(method_table, "getResourceType", &api_getResourceType);
			S.set_map_value(method_table, "getResourceName", &api_getResourceName);

			// metatable

			auto const metatable = S.create_metatable(ClassID);
			S.set_map_value(metatable, "__gc", &api___gc);
			S.set_map_value(metatable, "__tostring", &api___tostring);
			S.set_map_value(metatable, "__eq", &api___eq);
			S.set_map_value(metatable, "__index", method_table);

			// factory

			// 暂时不暴露出创建接口
			//auto const class_table = S.create_map();
			//S.set_map_value(class_table, "createFromFile", &api_createFromFile);

			// register

			// 暂时不暴露出创建接口
			//auto const M = S.push_module("LuaSTG.Sub");
			//S.set_map_value(M, "Texture2D", class_table);
		}
	};

	struct ResourcePoolBinding
	{
		static constexpr std::string_view const ClassID{ "LuaSTG.Sub.ResourcePool" };

		luastg::ResourcePoolId id;

		static luastg::ResourcePool* getPool(lua_State* L, ResourcePoolBinding* self)
		{
			auto* pool = LRES.GetResourcePool(self->id);
			if (!pool) {
				luaL_error(L, "resource pool has been destroyed");
			}
			return pool;
		}

		static int api_createTextureFromFile(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			auto const name = S.get_value<std::string_view>(2);
			auto const path = S.get_value<std::string_view>(3);
			auto const mipmap = S.get_value<bool>(4, true);
			auto* pool = getPool(L, self);
			if (!pool->LoadTexture(name.data(), path.data(), mipmap)) {
				return luaL_error(L, "can't create texture '%s' from file '%s'.", name.data(), path.data());
			}
			auto res = pool->GetTexture(name);
			auto* tex = ResourceTexture::create(L);
			tex->data = res.detach(); // 转移所有权
			return 1;
		}
		static int api_createSprite(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			auto const sprite_name = S.get_value<std::string_view>(2);
			core::SmartReference<luastg::IResourceTexture> texture = ResourceTexture::cast(L, 3)->data;
			auto const x = S.get_value<float>(4, 0.0f);
			auto const y = S.get_value<float>(5, 0.0f);
			auto const texture_size = texture->GetTexture()->getSize();
			auto const width = S.get_value<float>(6, float(texture_size.x));
			auto const height = S.get_value<float>(7, float(texture_size.y));
			auto const a = S.get_value<float>(8, 0.0f);
			auto const b = S.get_value<float>(9, 0.0f);
			auto const rect = S.get_value<bool>(10, false);
			auto* pool = getPool(L, self);
			if (!pool->CreateSprite(sprite_name.data(), texture.get(), x, y, width, height, a, b, rect))
			{
				return luaL_error(L, "load image failed (name='%s', tex='%s').", sprite_name.data(), texture->GetResName().data());
			}
			auto res = pool->GetSprite(sprite_name);
			auto* sprite = ResourceSprite::create(L);
			sprite->data = res.detach(); // 转移所有权
			return 1;
		}
		static int api_createSpriteSequence(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);

			auto const sprite_sequence_name = S.get_value<std::string_view>(2);
			if (ResourceTexture::test(L, 3)) {
				core::SmartReference<luastg::IResourceTexture> texture;
				texture = ResourceTexture::cast(L, 3)->data;
				auto const x = S.get_value<float>(4);
				auto const y = S.get_value<float>(5);
				auto const width = S.get_value<float>(6);
				auto const height = S.get_value<float>(7);
				auto const columns = S.get_value<int32_t>(8);
				auto const rows = S.get_value<int32_t>(9);
				auto const interval = S.get_value<int32_t>(10);
				auto const a = S.get_value<float>(11, 0.0f);
				auto const b = S.get_value<float>(12, 0.0f);
				auto const rect = S.get_value<bool>(13, false);
				if (!getPool(L, self)->CreateAnimation(
					sprite_sequence_name.data(), texture.get(),
					x, y, width, height,
					columns, rows,
					interval,
					a, b, rect))
				{
					return luaL_error(L, "load animation failed (name='%s', tex='%s').", sprite_sequence_name.data(), texture->GetResName());
				}
			}
			else /* (S.is_table(3)) */ {
				size_t const sprite_count = S.get_array_size(3);
				std::vector<core::SmartReference<luastg::IResourceSprite>> sprite_list;
				for (size_t index = 0; index < sprite_count; index += 1)
				{
					S.push_array_value_zero_base(3, index);
					auto* p_sprite = ResourceSprite::cast(L, -1);
					S.pop_value();
					sprite_list.emplace_back(p_sprite->data);
				}
				auto const interval = S.get_value<int32_t>(4);
				auto const a = S.get_value<float>(5, 0.0f);
				auto const b = S.get_value<float>(6, 0.0f);
				auto const rect = S.get_value<bool>(7, false);
				if (!getPool(L, self)->CreateAnimation(sprite_sequence_name.data(), sprite_list, interval, a, b, rect))
				{
					return luaL_error(L, "load animation failed (name='%s').", sprite_sequence_name.data());
				}
			}
			auto res = getPool(L, self)->GetAnimation(sprite_sequence_name);
			auto* sprite_sequence = ResourceSpriteSequence::create(L);
			sprite_sequence->data = res.detach(); // 转移所有权
			return 1;
		}
		static int api_removeTexture(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			core::SmartReference<luastg::IResourceTexture> texture;
			if (S.is_string(2)) {
				auto const texture_name = S.get_value<std::string_view>(2);
				texture = getPool(L, self)->GetTexture(texture_name);
			}
			else {
				auto* p_texture = ResourceTexture::cast(L, 2);
				texture = p_texture->data;
			}
			if (texture) {
				getPool(L, self)->RemoveResource(luastg::ResourceType::Texture, texture->GetResName().data());
			}
			return 0;
		}
		static int api_removeSprite(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			core::SmartReference<luastg::IResourceSprite> sprite;
			if (S.is_string(2)) {
				auto const sprite_name = S.get_value<std::string_view>(2);
				sprite = getPool(L, self)->GetSprite(sprite_name);
			}
			else {
				auto* p_sprite = ResourceSprite::cast(L, 2);
				sprite = p_sprite->data;
			}
			if (sprite) {
				getPool(L, self)->RemoveResource(luastg::ResourceType::Sprite, sprite->GetResName().data());
			}
			return 0;
		}
		static int api_removeSpriteSequence(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			core::SmartReference<luastg::IResourceAnimation> sprite_sequence;
			if (S.is_string(2)) {
				auto const sprite_sequence_name = S.get_value<std::string_view>(2);
				sprite_sequence = getPool(L, self)->GetAnimation(sprite_sequence_name);
			}
			else {
				auto* p_sprite_seq = ResourceSpriteSequence::cast(L, 2);
				sprite_sequence = p_sprite_seq->data;
			}
			if (sprite_sequence) {
				getPool(L, self)->RemoveResource(luastg::ResourceType::Animation, sprite_sequence->GetResName().data());
			}
			return 0;
		}
		static int api_getTexture(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			auto const texture_name = S.get_value<std::string_view>(2);
			auto res = getPool(L, self)->GetTexture(texture_name);
			if (!res) {
				return luaL_error(L, "can't find texture '%s'.", texture_name.data());
			}
			auto* tex = ResourceTexture::create(L);
			tex->data = res.detach(); // 转移所有权
			return 1;
		}
		static int api_getSprite(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			auto const sprite_name = S.get_value<std::string_view>(2);
			auto res = getPool(L, self)->GetSprite(sprite_name);
			if (!res) {
				return luaL_error(L, "can't find sprite '%s'.", sprite_name.data());
			}
			auto* sprite = ResourceSprite::create(L);
			sprite->data = res.detach(); // 转移所有权
			return 1;
		}
		static int api_getSpriteSequence(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			auto const sprite_sequence_name = S.get_value<std::string_view>(2);
			auto res = getPool(L, self)->GetAnimation(sprite_sequence_name);
			if (!res) {
				return luaL_error(L, "can't find animation '%s'.", sprite_sequence_name.data());
			}
			auto* sprite_sequence = ResourceSpriteSequence::create(L);
			sprite_sequence->data = res.detach(); // 转移所有权
			return 1;
		}
		static int api_isTextureExist(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			auto const texture_name = S.get_value<std::string_view>(2);
			auto res = getPool(L, self)->GetTexture(texture_name);
			S.push_value(static_cast<bool>(res));
			return 1;
		}
		static int api_isSpriteExist(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			auto const sprite_name = S.get_value<std::string_view>(2);
			auto res = getPool(L, self)->GetSprite(sprite_name);
			S.push_value(static_cast<bool>(res));
			return 1;
		}
		static int api_isSpriteSequenceExist(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			auto const sprite_sequence_name = S.get_value<std::string_view>(2);
			auto res = getPool(L, self)->GetAnimation(sprite_sequence_name);
			S.push_value(static_cast<bool>(res));
			return 1;
		}

		static int api___gc(lua_State* L)
		{
			std::ignore = cast(L, 1);
			return 0;
		}
		static int api_getName(lua_State* L)
		{
			auto* self = cast(L, 1);
			auto* pool = getPool(L, self);
			lua_pushlstring(L, pool->GetName().data(), pool->GetName().size());
			return 1;
		}
		static int api_isValid(lua_State* L)
		{
			auto* self = cast(L, 1);
			lua_pushboolean(L, LRES.GetResourcePool(self->id) != nullptr);
			return 1;
		}
		static int api_clear(lua_State* L)
		{
			getPool(L, cast(L, 1))->Clear();
			return 0;
		}
		static int api_remove(lua_State* L)
		{
			auto* pool = getPool(L, cast(L, 1));
			if (ResourceTexture::test(L, 2)) {
				auto* resource = ResourceTexture::cast(L, 2)->data;
				pool->RemoveResource(luastg::ResourceType::Texture, resource->GetResName().data());
			}
			else if (ResourceSprite::test(L, 2)) {
				auto* resource = ResourceSprite::cast(L, 2)->data;
				pool->RemoveResource(luastg::ResourceType::Sprite, resource->GetResName().data());
			}
			else if (ResourceSpriteSequence::test(L, 2)) {
				auto* resource = ResourceSpriteSequence::cast(L, 2)->data;
				pool->RemoveResource(luastg::ResourceType::Animation, resource->GetResName().data());
			}
			else {
				return luaL_error(L, "expected a resource object");
			}
			return 0;
		}
		static int api_removeByName(lua_State* L)
		{
			auto* pool = getPool(L, cast(L, 1));
			auto const type = static_cast<luastg::ResourceType>(luaL_checkinteger(L, 2));
			pool->RemoveResource(type, luaL_checkstring(L, 3));
			return 0;
		}
		static int api_contains(lua_State* L)
		{
			auto* pool = getPool(L, cast(L, 1));
			auto const type = static_cast<luastg::ResourceType>(luaL_checkinteger(L, 2));
			lua_pushboolean(L, pool->CheckResourceExists(type, luaL_checkstring(L, 3)));
			return 1;
		}
		static int api_enumerate(lua_State* L)
		{
			auto* pool = getPool(L, cast(L, 1));
			auto const type = static_cast<luastg::ResourceType>(luaL_checkinteger(L, 2));
			return pool->ExportResourceList(L, type);
		}
		static int api___tostring(lua_State* L)
		{
			lua::stack_t S(L);
			std::ignore = cast(L, 1);
			S.push_value<std::string_view>(ClassID);
			return 1;
		}
		static int api___eq(lua_State* L)
		{
			lua::stack_t S(L);
			auto* self = cast(L, 1);
			if (test(L, 2)) {
				auto* other = cast(L, 2);
				S.push_value(self->id == other->id);
			}
			else {
				S.push_value(false);
			}
			return 1;
		}

		static ResourcePoolBinding* create(lua_State* L)
		{
			lua::stack_t S(L);

			auto* self = S.create_userdata<ResourcePoolBinding>();
			auto const self_index = S.index_of_top();
			S.set_metatable(self_index, ClassID);

			self->id = luastg::InvalidResourcePoolId;
			return self;
		}
		static ResourcePoolBinding* cast(lua_State* L, int idx)
		{
			return static_cast<ResourcePoolBinding*>(luaL_checkudata(L, idx, ClassID.data()));
		}
		static bool test(lua_State* L, int idx)
		{
			return nullptr != luaL_testudata(L, idx, ClassID.data());
		}
		static void registerClass(lua_State* L)
		{
			[[maybe_unused]] lua::stack_balancer_t SB(L);
			lua::stack_t S(L);

			// method

			auto const method_table = S.create_map();
			S.set_map_value(method_table, "loadTexture", &api_createTextureFromFile);
			S.set_map_value(method_table, "createSprite", &api_createSprite);
			S.set_map_value(method_table, "createAnimation", &api_createSpriteSequence);
			S.set_map_value(method_table, "getName", &api_getName);
			S.set_map_value(method_table, "isValid", &api_isValid);
			S.set_map_value(method_table, "clear", &api_clear);
			S.set_map_value(method_table, "remove", &api_remove);
			S.set_map_value(method_table, "removeByName", &api_removeByName);
			S.set_map_value(method_table, "contains", &api_contains);
			S.set_map_value(method_table, "enumerate", &api_enumerate);
			S.set_map_value(method_table, "getTexture", &api_getTexture);
			S.set_map_value(method_table, "getSprite", &api_getSprite);
			S.set_map_value(method_table, "getAnimation", &api_getSpriteSequence);
			S.set_map_value(method_table, "hasTexture", &api_isTextureExist);
			S.set_map_value(method_table, "hasSprite", &api_isSpriteExist);
			S.set_map_value(method_table, "hasAnimation", &api_isSpriteSequenceExist);

			// metatable

			auto const metatable = S.create_metatable(ClassID);
			S.set_map_value(metatable, "__gc", &api___gc);
			S.set_map_value(metatable, "__tostring", &api___tostring);
			S.set_map_value(metatable, "__eq", &api___eq);
			S.set_map_value(metatable, "__index", method_table);

		}
	};

	struct ResourceManager
	{
		static constexpr std::string_view const ClassID{ "LuaSTG.Sub.ResourceManager" };

		static int api_createPool(lua_State* L)
		{
			lua::stack_t S(L);
			auto const name = S.get_value<std::string_view>(1);
			auto const id = LRES.CreateResourcePool(name);
			if (id == luastg::InvalidResourcePoolId) {
				return luaL_error(L, "resource pool name must be non-empty and unique");
			}
			auto* set = ResourcePoolBinding::create(L);
			set->id = id;
			return 1;
		}
		static int api_getPool(lua_State* L)
		{
			lua::stack_t S(L);
			auto const name = S.get_value<std::string_view>(1);
			auto* pool = LRES.GetResourcePool(name);
			if (!pool) {
				lua_pushnil(L);
				return 1;
			}
			auto* set = ResourcePoolBinding::create(L);
			set->id = pool->GetId();
			return 1;
		}
		static int api_destroyPool(lua_State* L)
		{
			auto* pool = ResourcePoolBinding::cast(L, 1);
			if (!LRES.DestroyResourcePool(pool->id)) {
				return luaL_error(L, "resource pool has already been destroyed");
			}
			return 0;
		}
		static int api_setLookupOrder(lua_State* L)
		{
			luaL_checktype(L, 1, LUA_TTABLE);
			std::vector<luastg::ResourcePoolId> order;
			size_t const count = lua_objlen(L, 1);
			order.reserve(count);
			for (size_t i = 1; i <= count; ++i) {
				lua_rawgeti(L, 1, static_cast<int>(i));
				order.push_back(ResourcePoolBinding::cast(L, -1)->id);
				lua_pop(L, 1);
			}
			if (!LRES.SetLookupOrder(order)) {
				return luaL_error(L, "lookup order requires unique, live resource pools");
			}
			return 0;
		}
		static int pushPools(lua_State* L, std::vector<luastg::ResourcePoolId> const& ids)
		{
			lua_createtable(L, static_cast<int>(ids.size()), 0);
			for (size_t i = 0; i < ids.size(); ++i) {
				auto* set = ResourcePoolBinding::create(L);
				set->id = ids[i];
				lua_rawseti(L, -2, static_cast<int>(i + 1));
			}
			return 1;
		}
		static int api_getLookupOrder(lua_State* L)
		{
			return pushPools(L, LRES.GetLookupOrder());
		}
		static int api_getPools(lua_State* L)
		{
			std::vector<luastg::ResourcePoolId> ids;
			for (auto* pool : LRES.GetResourcePools()) {
				ids.push_back(pool->GetId());
			}
			return pushPools(L, ids);
		}

		static void registerClass(lua_State* L)
		{
			[[maybe_unused]] lua::stack_balancer_t SB(L);
			lua::stack_t S(L);

			// class

			auto const class_table = S.create_map();
			S.set_map_value(class_table, "createPool", &api_createPool);
			S.set_map_value(class_table, "getPool", &api_getPool);
			S.set_map_value(class_table, "destroyPool", &api_destroyPool);
			S.set_map_value(class_table, "setLookupOrder", &api_setLookupOrder);
			S.set_map_value(class_table, "getLookupOrder", &api_getLookupOrder);
			S.set_map_value(class_table, "getPools", &api_getPools);

			// register

			auto const M = S.push_module("lstg");
			S.set_map_value(M, "ResourceManager", class_table);
		}
	};
}

luastg::ResourcePoolId luastg::binding::checkResourcePoolId(lua_State* L, int const index)
{
	return ResourcePoolBinding::cast(L, index)->id;
}

luastg::ResourcePool* luastg::binding::checkResourcePool(lua_State* L, int const index)
{
	auto* handle = ResourcePoolBinding::cast(L, index);
	return ResourcePoolBinding::getPool(L, handle);
}

void luastg::binding::pushResourcePool(lua_State* L, ResourcePoolId const id)
{
	auto* handle = ResourcePoolBinding::create(L);
	handle->id = id;
}

void luastg::binding::registerResourcePoolMethods(lua_State* L, luaL_Reg const* methods)
{
	luaL_getmetatable(L, ResourcePoolBinding::ClassID.data());
	lua_getfield(L, -1, "__index");
	luaL_register(L, nullptr, methods);
	lua_pop(L, 2);
}

luastg::IResourceTexture* luastg::binding::checkResourceTexture(lua_State* L, int const index)
{
	return ResourceTexture::cast(L, index)->data;
}

luastg::IResourceSprite* luastg::binding::checkResourceSprite(lua_State* L, int const index)
{
	return ResourceSprite::cast(L, index)->data;
}

void luastg::binding::pushResourceTexture(lua_State* L, IResourceTexture* resource)
{
	resource->retain();
	ResourceTexture::create(L)->data = resource;
}

void luastg::binding::pushResourceSprite(lua_State* L, IResourceSprite* resource)
{
	resource->retain();
	ResourceSprite::create(L)->data = resource;
}

void luastg::binding::pushResourceAnimation(lua_State* L, IResourceAnimation* resource)
{
	resource->retain();
	ResourceSpriteSequence::create(L)->data = resource;
}

int luaopen_LuaSTG_Sub(lua_State* L)
{
	luastg::binding::ResourceTexture::registerClass(L);
	luastg::binding::ResourceSprite::registerClass(L);
	luastg::binding::ResourceSpriteSequence::registerClass(L);
	luastg::binding::ResourcePoolBinding::registerClass(L);
	luastg::binding::ResourceManager::registerClass(L);
	return 1;
}
