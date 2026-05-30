#include "lua_sqlite.hpp"

#include "sqlite3.h"

#include <cstring>

namespace
{
	const char* LIB_NAME = "sqlite";
	const char* DB_META = "sqlite.db";
	const char* NULL_META = "sqlite.null";
	char NULL_KEY;

	struct db_t
	{
		sqlite3* db;
	};

	static int abs_index(lua_State* L, int idx)
	{
		if (idx > 0 || idx <= LUA_REGISTRYINDEX)
		{
			return idx;
		}
		return lua_gettop(L) + idx + 1;
	}

	static int push_error(lua_State* L, sqlite3* db, const char* what, int rc)
	{
		lua_pushfstring(L, "sqlite %s error %d: %s", what, rc, db ? sqlite3_errmsg(db) : "");
		return lua_error(L);
	}

	static db_t* check_db(lua_State* L)
	{
		db_t* self = (db_t*)luaL_checkudata(L, 1, DB_META);
		if (!self->db)
		{
			luaL_error(L, "sqlite database is closed");
		}
		return self;
	}

	static db_t* check_db_any(lua_State* L)
	{
		return (db_t*)luaL_checkudata(L, 1, DB_META);
	}

	static int null_tostring(lua_State* L)
	{
		lua_pushliteral(L, "sqlite.null");
		return 1;
	}

	static void new_null(lua_State* L)
	{
		lua_newuserdata(L, 0);
		if (luaL_newmetatable(L, NULL_META))
		{
			lua_pushcfunction(L, null_tostring);
			lua_setfield(L, -2, "__tostring");
		}
		lua_setmetatable(L, -2);
	}

	static void push_null(lua_State* L)
	{
		lua_pushlightuserdata(L, &NULL_KEY);
		lua_gettable(L, LUA_REGISTRYINDEX);
		if (!lua_isnil(L, -1))
		{
			return;
		}
		lua_pop(L, 1);

		new_null(L);
		lua_pushlightuserdata(L, &NULL_KEY);
		lua_pushvalue(L, -2);
		lua_settable(L, LUA_REGISTRYINDEX);
	}

	static bool is_null(lua_State* L, int idx)
	{
		idx = abs_index(L, idx);
		lua_pushlightuserdata(L, &NULL_KEY);
		lua_gettable(L, LUA_REGISTRYINDEX);
		bool ret = lua_rawequal(L, idx, -1) != 0;
		lua_pop(L, 1);
		return ret;
	}

	static int get_open_flags(lua_State* L)
	{
		if (lua_isnoneornil(L, 2))
		{
			return SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
		}

		const char* mode = luaL_checkstring(L, 2);
		if (strcmp(mode, "r") == 0 || strcmp(mode, "readonly") == 0)
		{
			return SQLITE_OPEN_READONLY;
		}
		if (strcmp(mode, "rw") == 0 || strcmp(mode, "readwrite") == 0)
		{
			return SQLITE_OPEN_READWRITE;
		}
		if (strcmp(mode, "rwc") == 0 || strcmp(mode, "create") == 0 || strcmp(mode, "readwrite_create") == 0)
		{
			return SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
		}
		return luaL_error(L, "invalid sqlite open mode '%s'", mode);
	}

	static int close_db(sqlite3*& db)
	{
		if (!db)
		{
			return SQLITE_OK;
		}
		sqlite3* old = db;
		int rc = sqlite3_close(old);
		if (rc == SQLITE_OK)
		{
			db = nullptr;
		}
		return rc;
	}

	static int bind_one(lua_State* L, sqlite3_stmt* stmt, int n, int idx)
	{
		int rc = SQLITE_OK;
		if (lua_isnil(L, idx) || is_null(L, idx))
		{
			rc = sqlite3_bind_null(stmt, n);
		}
		else if (lua_isboolean(L, idx))
		{
			rc = sqlite3_bind_int(stmt, n, lua_toboolean(L, idx) ? 1 : 0);
		}
		else if (lua_isnumber(L, idx))
		{
			lua_Number num = lua_tonumber(L, idx);
			lua_Integer i = lua_tointeger(L, idx);
			if (num == (lua_Number)i)
			{
				rc = sqlite3_bind_int64(stmt, n, (sqlite3_int64)i);
			}
			else
			{
				rc = sqlite3_bind_double(stmt, n, (double)num);
			}
		}
		else if (lua_isstring(L, idx))
		{
			size_t len = 0;
			const char* text = lua_tolstring(L, idx, &len);
			rc = sqlite3_bind_text(stmt, n, text, (int)len, SQLITE_TRANSIENT);
		}
		else
		{
			lua_pushfstring(L, "unsupported sqlite parameter type '%s'", luaL_typename(L, idx));
			return 1;
		}

		if (rc != SQLITE_OK)
		{
			lua_pushfstring(L, "sqlite bind error %d: %s", rc, sqlite3_errmsg(sqlite3_db_handle(stmt)));
			return 1;
		}
		return 0;
	}

	static int bind_params(lua_State* L, sqlite3_stmt* stmt)
	{
		int count = sqlite3_bind_parameter_count(stmt);
		if (lua_isnoneornil(L, 3))
		{
			if (count != 0)
			{
				lua_pushfstring(L, "sqlite expected %d parameter(s), got 0", count);
				return 1;
			}
			return 0;
		}

		if (!lua_istable(L, 3))
		{
			lua_pushfstring(L, "sqlite parameters must be a table, got %s", luaL_typename(L, 3));
			return 1;
		}
		int got = (int)lua_objlen(L, 3);
		if (got != count)
		{
			lua_pushfstring(L, "sqlite expected %d parameter(s), got %d", count, got);
			return 1;
		}

		int params = abs_index(L, 3);
		for (int i = 1; i <= count; i += 1)
		{
			lua_rawgeti(L, params, i);
			int err = bind_one(L, stmt, i, -1);
			if (err != 0)
			{
				lua_remove(L, -2);
				return err;
			}
			lua_pop(L, 1);
		}
		return 0;
	}

	static sqlite3_stmt* prepare(lua_State* L, sqlite3* db, const char* sql)
	{
		sqlite3_stmt* stmt = nullptr;
		int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
		if (rc != SQLITE_OK)
		{
			push_error(L, db, "prepare", rc);
		}
		if (!stmt)
		{
			luaL_error(L, "sqlite statement is empty");
		}
		return stmt;
	}

	static void push_column(lua_State* L, sqlite3_stmt* stmt, int col)
	{
		switch (sqlite3_column_type(stmt, col))
		{
		case SQLITE_INTEGER:
			lua_pushinteger(L, (lua_Integer)sqlite3_column_int64(stmt, col));
			break;
		case SQLITE_FLOAT:
			lua_pushnumber(L, (lua_Number)sqlite3_column_double(stmt, col));
			break;
		case SQLITE_TEXT:
		{
			const char* text = (const char*)sqlite3_column_text(stmt, col);
			lua_pushlstring(L, text ? text : "", (size_t)sqlite3_column_bytes(stmt, col));
			break;
		}
		case SQLITE_BLOB:
		{
			const void* data = sqlite3_column_blob(stmt, col);
			int size = sqlite3_column_bytes(stmt, col);
			lua_pushlstring(L, data ? (const char*)data : "", (size_t)size);
			break;
		}
		case SQLITE_NULL:
		default:
			push_null(L);
			break;
		}
	}

	static int db_close(lua_State* L)
	{
		db_t* self = check_db_any(L);
		int rc = close_db(self->db);
		if (rc != SQLITE_OK)
		{
			return luaL_error(L, "sqlite close error %d: database is busy", rc);
		}
		return 0;
	}

	static int db_gc(lua_State* L)
	{
		db_t* self = check_db_any(L);
		close_db(self->db);
		return 0;
	}

	static int db_is_open(lua_State* L)
	{
		db_t* self = check_db_any(L);
		lua_pushboolean(L, self->db ? 1 : 0);
		return 1;
	}

	static int db_exec(lua_State* L)
	{
		db_t* self = check_db(L);
		const char* sql = luaL_checkstring(L, 2);
		sqlite3_stmt* stmt = prepare(L, self->db, sql);

		if (bind_params(L, stmt) != 0)
		{
			sqlite3_finalize(stmt);
			return lua_error(L);
		}

		for (;;)
		{
			int rc = sqlite3_step(stmt);
			if (rc == SQLITE_DONE)
			{
				break;
			}
			if (rc != SQLITE_ROW)
			{
				lua_pushfstring(L, "sqlite exec error %d: %s", rc, sqlite3_errmsg(self->db));
				sqlite3_finalize(stmt);
				return lua_error(L);
			}
		}

		sqlite3* db = sqlite3_db_handle(stmt);
		int rc = sqlite3_finalize(stmt);
		if (rc != SQLITE_OK)
		{
			return push_error(L, db, "exec", rc);
		}
		lua_pushboolean(L, 1);
		return 1;
	}

	static int db_query(lua_State* L)
	{
		db_t* self = check_db(L);
		const char* sql = luaL_checkstring(L, 2);
		sqlite3_stmt* stmt = prepare(L, self->db, sql);

		if (bind_params(L, stmt) != 0)
		{
			sqlite3_finalize(stmt);
			return lua_error(L);
		}

		lua_newtable(L);
		int row = 1;
		for (;;)
		{
			int rc = sqlite3_step(stmt);
			if (rc == SQLITE_DONE)
			{
				break;
			}
			if (rc != SQLITE_ROW)
			{
				lua_pushfstring(L, "sqlite query error %d: %s", rc, sqlite3_errmsg(self->db));
				sqlite3_finalize(stmt);
				return lua_error(L);
			}

			lua_newtable(L);
			int cols = sqlite3_column_count(stmt);
			for (int i = 0; i < cols; i += 1)
			{
				const char* name = sqlite3_column_name(stmt, i);
				if (name && name[0])
				{
					lua_pushstring(L, name);
				}
				else
				{
					lua_pushfstring(L, "column%d", i + 1);
				}
				push_column(L, stmt, i);
				lua_settable(L, -3);
			}
			lua_rawseti(L, -2, row);
			row += 1;
		}

		sqlite3* db = sqlite3_db_handle(stmt);
		int rc = sqlite3_finalize(stmt);
		if (rc != SQLITE_OK)
		{
			return push_error(L, db, "query", rc);
		}
		return 1;
	}

	static int exec_sql(lua_State* L, db_t* self, const char* sql)
	{
		char* msg = nullptr;
		int rc = sqlite3_exec(self->db, sql, nullptr, nullptr, &msg);
		if (rc != SQLITE_OK)
		{
			lua_pushfstring(L, "sqlite error %d: %s", rc, msg ? msg : sqlite3_errmsg(self->db));
			sqlite3_free(msg);
			return lua_error(L);
		}
		return 0;
	}

	static int db_transaction(lua_State* L)
	{
		db_t* self = check_db(L);
		luaL_checktype(L, 2, LUA_TFUNCTION);
		lua_settop(L, 2);

		exec_sql(L, self, "BEGIN");

		lua_pushvalue(L, 2);
		lua_pushvalue(L, 1);
		if (lua_pcall(L, 1, LUA_MULTRET, 0) != 0)
		{
			const char* msg = lua_tostring(L, -1);
			sqlite3_exec(self->db, "ROLLBACK", nullptr, nullptr, nullptr);
			lua_pushfstring(L, "sqlite transaction error: %s", msg ? msg : "");
			return lua_error(L);
		}

		int results = lua_gettop(L) - 2;
		char* msg = nullptr;
		int rc = sqlite3_exec(self->db, "COMMIT", nullptr, nullptr, &msg);
		if (rc != SQLITE_OK)
		{
			lua_pushfstring(L, "sqlite commit error %d: %s", rc, msg ? msg : sqlite3_errmsg(self->db));
			sqlite3_free(msg);
			sqlite3_exec(self->db, "ROLLBACK", nullptr, nullptr, nullptr);
			return lua_error(L);
		}
		return results;
	}

	static int db_tostring(lua_State* L)
	{
		db_t* self = check_db_any(L);
		lua_pushfstring(L, "sqlite.db: %s", self->db ? "open" : "closed");
		return 1;
	}

	static void register_db(lua_State* L)
	{
		luaL_Reg mt[] = {
			{ "__gc", db_gc },
			{ "__tostring", db_tostring },
			{ NULL, NULL },
		};
		luaL_Reg methods[] = {
			{ "close", db_close },
			{ "isOpen", db_is_open },
			{ "exec", db_exec },
			{ "query", db_query },
			{ "transaction", db_transaction },
			{ NULL, NULL },
		};

		if (luaL_newmetatable(L, DB_META))
		{
			luaL_register(L, NULL, mt);
			lua_newtable(L);
			luaL_register(L, NULL, methods);
			lua_setfield(L, -2, "__index");
		}
		lua_pop(L, 1);
	}

	static int sqlite_open(lua_State* L)
	{
		const char* path = luaL_checkstring(L, 1);
		int flags = get_open_flags(L);
		sqlite3* db = nullptr;

		int rc = sqlite3_open_v2(path, &db, flags, nullptr);
		if (rc != SQLITE_OK)
		{
			lua_pushfstring(L, "sqlite open error %d: %s", rc, db ? sqlite3_errmsg(db) : "");
			if (db)
			{
				sqlite3_close(db);
			}
			return lua_error(L);
		}

		db_t* self = (db_t*)lua_newuserdata(L, sizeof(db_t));
		self->db = db;
		luaL_getmetatable(L, DB_META);
		lua_setmetatable(L, -2);
		return 1;
	}
}

int luaopen_sqlite(lua_State* L)
{
	register_db(L);

	luaL_Reg lib[] = {
		{ "open", sqlite_open },
		{ NULL, NULL },
	};
	luaL_register(L, LIB_NAME, lib);

	lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
	lua_pushvalue(L, -2);
	lua_setfield(L, -2, LIB_NAME);
	lua_pop(L, 1);

	push_null(L);
	lua_setfield(L, -2, "null");

	lua_pushliteral(L, SQLITE_VERSION);
	lua_setfield(L, -2, "version");
	lua_pushinteger(L, SQLITE_VERSION_NUMBER);
	lua_setfield(L, -2, "versionNumber");

	return 1;
}
