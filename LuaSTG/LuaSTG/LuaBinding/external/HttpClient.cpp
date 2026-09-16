#include "HttpClient.hpp"
#include "core/HttpClient.hpp"
#include <array>
#include <exception>
#include <new>
#include <optional>
#include <utility>

static int
verifyUrl(lua_State* L, std::string_view const& url)
{
    try {
        core::http::validateUrl(url);
        return 0;
    } catch(const std::exception& error) {
        lua_pushstring(L, error.what());
    }
    return lua_error(L);
}

static http::RequestMethod
verifyRequestMethod(lua_State* L, std::string_view const& request_method_name)
{
    struct RequestMethodMetadata
    {
        std::string_view name;
        http::RequestMethod request_method;
    };
    static constexpr std::array<RequestMethodMetadata, 9> metadata{
        {
            { "GET", http::RequestMethod::get },
            { "HEAD", http::RequestMethod::head },
            { "POST", http::RequestMethod::post },
            { "PUT", http::RequestMethod::put },
            { "DELETE", http::RequestMethod::del },
            { "CONNECT", http::RequestMethod::custom },
            { "OPTIONS", http::RequestMethod::custom },
            { "TRACE", http::RequestMethod::custom },
            { "PATCH", http::RequestMethod::patch },
        }
    };
    for(const auto& [name, request_method] : metadata) {
        if(request_method_name == name) {
            return request_method;
        }
    }
    luaL_error(L, "unknown request method '%s'", request_method_name.data());
    return http::RequestMethod::custom;
}

static std::string_view
getRequestMethodName(http::RequestMethod const request_method)
{
    switch(request_method) {
        default:
            return { "UNKNOWN" };
        case http::RequestMethod::get:
            return { "GET" };
        case http::RequestMethod::head:
            return { "HEAD" };
        case http::RequestMethod::post:
            return { "POST" };
        case http::RequestMethod::put:
            return { "PUT" };
        case http::RequestMethod::del:
            return { "DELETE" };
        case http::RequestMethod::patch:
            return { "PATCH" };
    }
}

namespace lua
{
    struct StackIndex
    {
        int32_t value{};

        explicit StackIndex(int32_t const value) noexcept
            : value(value)
        {
        }
    };

    class StackBalancer
    {
    public:
        explicit StackBalancer(lua_State* L)
            : L(L), N(lua_gettop(L))
        {
        }

        ~StackBalancer() { lua_settop(L, N); }

    private:
        lua_State* L;
        int32_t N;
    };

    class Stack
    {
    public:
        explicit Stack(lua_State* L) noexcept
            : L(L)
        {
        }

        [[nodiscard]] StackIndex indexOfTop() const
        {
            return StackIndex(lua_gettop(L));
        }

        void pushValue(std::nullopt_t const) const
        {
            lua_pushnil(L);
        }

        void pushValue(bool const value) const
        {
            lua_pushboolean(L, value ? 1 : 0);
        }

        void pushValue(std::string_view const& value) const
        {
            lua_pushlstring(L, value.data(), value.length());
        }

        void pushValue(StackIndex const value) const
        {
            lua_pushvalue(L, value.value);
        }

        template<typename T>
        [[nodiscard]] T getValue(StackIndex index) const;

        template<typename T>
        [[nodiscard]] T getValue(int32_t const index) const
        {
            return getValue<T>(StackIndex(index));
        }

        // module and class system

        [[nodiscard]] StackIndex pushModule(std::string_view const& name) const
        {
            constexpr luaL_Reg empty[] = { {} };
            luaL_register(L, name.data(), empty);
            auto const index = lua_gettop(L);
            lua_pushnil(L);
            lua_setglobal(L, name.data());
            return StackIndex(index);
        }

        [[nodiscard]] StackIndex createMetaTable(std::string_view const& name) const
        {
            luaL_newmetatable(L, name.data());
            return StackIndex(lua_gettop(L));
        }

        void setMetaTable(StackIndex const index, std::string_view const& name) const
        {
            luaL_getmetatable(L, name.data());
            lua_setmetatable(L, index.value);
        }

        template<typename T>
        [[nodiscard]] T* createUserData() const
        {
            return static_cast<T*>(lua_newuserdata(L, sizeof(T)));
        }

        template<typename T>
        [[nodiscard]] T* createUserDataWithNew() const
        {
            auto p = static_cast<T*>(lua_newuserdata(L, sizeof(T)));
            return new(p) T();
        }

        // map

        [[nodiscard]] StackIndex createMap(size_t const reserve = 0) const
        {
            lua_createtable(L, 0, static_cast<int>(reserve));
            return indexOfTop();
        }

        void setMapValue(StackIndex const index, std::string_view const& key, lua_CFunction const value) const
        {
            lua_pushcfunction(L, value);
            lua_setfield(L, index.value, key.data());
        }

        void setMapValue(StackIndex const index, std::string_view const& key, StackIndex const value_index) const
        {
            lua_pushvalue(L, value_index.value);
            lua_setfield(L, index.value, key.data());
        }

    private:
        lua_State* L{};
    };

    template<>
    [[nodiscard]] int32_t Stack::getValue<int32_t>(StackIndex const index) const
    {
        return static_cast<int32_t>(luaL_checkinteger(L, index.value));
    }

    template<>
    [[nodiscard]] std::string_view Stack::getValue<std::string_view>(StackIndex const index) const
    {
        size_t len{};
        auto str = luaL_checklstring(L, index.value, &len);
        // managed by lua VM
        // ReSharper disable once CppDFALocalValueEscapesFunction
        return { str, len };
    }

    template<>
    [[nodiscard]] std::string Stack::getValue<std::string>(StackIndex const index) const
    {
        size_t len{};
        auto str = luaL_checklstring(L, index.value, &len);
        // managed by lua VM
        // ReSharper disable once CppDFALocalValueEscapesFunction
        return { str, len };
    }

    static_assert(sizeof(Stack) == sizeof(lua_State*));
    static_assert(alignof(Stack) == alignof(lua_State*));
}

namespace http
{
    std::string_view Request::class_name{ "http.Request" };

    struct RequestBinding : Request
    {
        // meta methods

        static int /* NOLINT(*-reserved-identifier) */ __gc(lua_State* L)
        {
            auto const self = as(L, 1);
            self->~Request();
            return 0;
        }

        static int /* NOLINT(*-reserved-identifier) */ __tostring(lua_State* L)
        {
            lua::Stack const S(L);
            [[maybe_unused]] auto const self = as(L, 1);
            S.pushValue(class_name);
            return 1;
        }

        // request methods

        static int setResolveTimeout(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = as(L, 1);
            auto const timeout = S.getValue<int32_t>(2);
            self->resolve_timeout = timeout;
            S.pushValue(lua::StackIndex(1)); // return self
            return 1;
        }

        static int setConnectTimeout(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = as(L, 1);
            auto const timeout = S.getValue<int32_t>(2);
            self->connect_timeout = timeout;
            S.pushValue(lua::StackIndex(1)); // return self
            return 1;
        }

        static int setSendTimeout(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = as(L, 1);
            auto const timeout = S.getValue<int32_t>(2);
            self->send_timeout = timeout;
            S.pushValue(lua::StackIndex(1)); // return self
            return 1;
        }

        static int setReceiveTimeout(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = as(L, 1);
            auto const timeout = S.getValue<int32_t>(2);
            self->receive_timeout = timeout;
            S.pushValue(lua::StackIndex(1)); // return self
            return 1;
        }

        static int addHeader(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = as(L, 1);
            auto const name = S.getValue<std::string_view>(2);
            auto const value = S.getValue<std::string_view>(3);
            self->headers.emplace(name, value);
            S.pushValue(lua::StackIndex(1)); // return self
            return 1;
        }

        static int body(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = as(L, 1);
            auto const body = S.getValue<std::string_view>(2);
            switch(self->request_method) {
                default:
                    return luaL_error(L, "request method '%s' does not support request body", getRequestMethodName(self->request_method).data());
                case RequestMethod::post:
                case RequestMethod::put:
                case RequestMethod::patch:
                    break;
            }
            self->body = body;
            S.pushValue(lua::StackIndex(1)); // return self
            return 1;
        }

        static int execute(lua_State* L)
        {
            auto const self = as(L, 1);
            try {
                core::http::Request request;
                request.url = self->url;
                request.method = self->request_method == RequestMethod::custom ? self->custom_request_method : std::string(getRequestMethodName(self->request_method));
                request.headers = self->headers;
                request.body = self->body;
                request.timeouts = { self->resolve_timeout, self->connect_timeout, self->send_timeout, self->receive_timeout };
                auto result = core::http::execute(request);
                auto* const response = ResponseEntity::create(L);
                response->headers = std::move(result.headers);
                response->body = std::move(result.body);
                return 1;
            } catch(const std::exception& error) {
                lua_pushstring(L, error.what());
            }
            return lua_error(L);
        }

        // static methods

        static int get(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = create(L);
            auto const url = S.getValue<std::string_view>(1);
            verifyUrl(L, url);
            self->request_method = RequestMethod::get;
            self->url = url;
            return 1;
        }

        static int head(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = create(L);
            auto const url = S.getValue<std::string_view>(1);
            verifyUrl(L, url);
            self->request_method = RequestMethod::head;
            self->url = url;
            return 1;
        }

        static int post(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = create(L);
            auto const url = S.getValue<std::string_view>(1);
            verifyUrl(L, url);
            self->request_method = RequestMethod::post;
            self->url = url;
            return 1;
        }

        static int put(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = create(L);
            auto const url = S.getValue<std::string_view>(1);
            verifyUrl(L, url);
            self->request_method = RequestMethod::put;
            self->url = url;
            return 1;
        }

        static int del(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = create(L);
            auto const url = S.getValue<std::string_view>(1);
            verifyUrl(L, url);
            self->request_method = RequestMethod::del;
            self->url = url;
            return 1;
        }

        static int patch(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = create(L);
            auto const url = S.getValue<std::string_view>(1);
            verifyUrl(L, url);
            self->request_method = RequestMethod::patch;
            self->url = url;
            return 1;
        }

        static int request(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = create(L);
            auto const request_method = S.getValue<std::string_view>(1);
            auto const url = S.getValue<std::string_view>(2);
            verifyUrl(L, url);
            self->request_method = verifyRequestMethod(L, request_method);
            self->custom_request_method = request_method;
            self->url = url;
            return 1;
        }
    };

    bool Request::is(lua_State* L, int const index)
    {
        if(lua_type(L, index) != LUA_TUSERDATA) {
            return false;
        }
        if(lua_getmetatable(L, index) == 0) {
            return false;
        }
        luaL_getmetatable(L, class_name.data());
        bool const equal = lua_equal(L, -2, -1) == 1;
        lua_pop(L, 2);
        return equal;
    }

    Request* Request::as(lua_State* L, int const index)
    {
        return static_cast<Request*>(luaL_checkudata(L, index, class_name.data()));
    }

    Request* Request::create(lua_State* L)
    {
        lua::Stack const S(L);
        auto const self = S.createUserDataWithNew<Request>();
        S.setMetaTable(S.indexOfTop(), class_name);
        return self;
    }

    void Request::registerClass(lua_State* L)
    {
        [[maybe_unused]] lua::StackBalancer const SB(L);
        lua::Stack const S(L);

        // method

        auto const methods = S.pushModule(class_name);
        S.setMapValue(methods, "setResolveTimeout", &RequestBinding::setResolveTimeout);
        S.setMapValue(methods, "setConnectTimeout", &RequestBinding::setConnectTimeout);
        S.setMapValue(methods, "setSendTimeout", &RequestBinding::setSendTimeout);
        S.setMapValue(methods, "setReceiveTimeout", &RequestBinding::setReceiveTimeout);
        S.setMapValue(methods, "addHeader", &RequestBinding::addHeader);
        S.setMapValue(methods, "body", &RequestBinding::body);
        S.setMapValue(methods, "execute", &RequestBinding::execute);
        S.setMapValue(methods, "get", &RequestBinding::get);
        S.setMapValue(methods, "head", &RequestBinding::head);
        S.setMapValue(methods, "post", &RequestBinding::post);
        S.setMapValue(methods, "put", &RequestBinding::put);
        S.setMapValue(methods, "delete", &RequestBinding::del); // delete is a c/c++ keyword
        S.setMapValue(methods, "patch", &RequestBinding::patch);
        S.setMapValue(methods, "request", &RequestBinding::request);

        // meta method

        auto const meta = S.createMetaTable(class_name);
        S.setMapValue(meta, "__gc", &RequestBinding::__gc);
        S.setMapValue(meta, "__tostring", &RequestBinding::__tostring);
        S.setMapValue(meta, "__index", methods);
    }
}

namespace http
{
    std::string_view ResponseEntity::class_name{ "http.ResponseEntity" };

    struct ResponseEntityBinding : ResponseEntity
    {
        // meta methods

        static int /* NOLINT(*-reserved-identifier) */ __gc(lua_State* L)
        {
            auto const self = as(L, 1);
            self->~ResponseEntity();
            return 0;
        }

        static int /* NOLINT(*-reserved-identifier) */ __tostring(lua_State* L)
        {
            lua::Stack const S(L);
            [[maybe_unused]] auto const self = as(L, 1);
            S.pushValue(class_name);
            return 1;
        }

        // methods

        static int hasHeader(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = as(L, 1);
            auto const name = S.getValue<std::string>(2);
            S.pushValue(self->headers.contains(name));
            return 1;
        }

        static int getHeader(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = as(L, 1);
            // ReSharper disable once CppTooWideScopeInitStatement
            auto const name = S.getValue<std::string>(2);
            if(self->headers.contains(name)) {
                S.pushValue(self->headers.at(name));
            } else {
                S.pushValue(std::nullopt);
            }
            return 1;
        }

        static int body(lua_State* L)
        {
            lua::Stack const S(L);
            auto const self = as(L, 1);
            S.pushValue(self->body);
            return 1;
        }
    };

    bool ResponseEntity::is(lua_State* L, int const index)
    {
        if(lua_type(L, index) != LUA_TUSERDATA) {
            return false;
        }
        if(lua_getmetatable(L, index) == 0) {
            return false;
        }
        luaL_getmetatable(L, class_name.data());
        bool const equal = lua_equal(L, -2, -1) == 1;
        lua_pop(L, 2);
        return equal;
    }

    ResponseEntity* ResponseEntity::as(lua_State* L, int const index)
    {
        return static_cast<ResponseEntity*>(luaL_checkudata(L, index, class_name.data()));
    }

    ResponseEntity* ResponseEntity::create(lua_State* L)
    {
        lua::Stack const S(L);
        auto const self = S.createUserDataWithNew<ResponseEntity>();
        S.setMetaTable(S.indexOfTop(), class_name);
        return self;
    }

    void ResponseEntity::registerClass(lua_State* L)
    {
        [[maybe_unused]] lua::StackBalancer const SB(L);
        lua::Stack const S(L);

        // method

        auto const methods = S.pushModule(class_name);
        S.setMapValue(methods, "hasHeader", &ResponseEntityBinding::hasHeader);
        S.setMapValue(methods, "getHeader", &ResponseEntityBinding::getHeader);
        S.setMapValue(methods, "body", &ResponseEntityBinding::body);

        // meta method

        auto const meta = S.createMetaTable(class_name);
        S.setMapValue(meta, "__gc", &ResponseEntityBinding::__gc);
        S.setMapValue(meta, "__tostring", &ResponseEntityBinding::__tostring);
        S.setMapValue(meta, "__index", methods);
    }
}
