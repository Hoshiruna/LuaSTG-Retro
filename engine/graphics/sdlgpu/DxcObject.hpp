#pragma once

#if defined(_WIN32)
#include <unknwn.h>
#endif
#include <dxcapi.h>
#include <memory>

namespace core::Graphics::SDLGPU
{
    template<typename T>
    struct ReleaseDxcObject
    {
        void operator()(T* value) const noexcept { value->Release(); }
    };

    template<typename T>
    using DxcObject = std::unique_ptr<T, ReleaseDxcObject<T>>;
}
