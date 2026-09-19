#include "ShaderSource.hpp"
#include "core/FileSystem.hpp"
#include "core/SmartReference.hpp"
#include <unknwn.h>
#include <dxcapi.h>
#include <wrl/client.h>
#include <atomic>
#include <filesystem>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace core::Graphics::SDLGPU
{
    namespace
    {
        using Microsoft::WRL::ComPtr;
        bool read(std::string_view path, std::string& source)
        {
            SmartReference<IData> data;
            if(!FileSystemManager::readFile(path, data.put()))
                return false;
            source.assign(static_cast<const char*>(data->data()), data->size());
            return true;
        }
        class Includes final : public IDxcIncludeHandler
        {
        public:
            explicit Includes(IDxcUtils* utils) : m_utils(utils) {}
            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** result) override
            {
                if(!result)
                    return E_POINTER;
                *result = nullptr;
                if(id != __uuidof(IUnknown) && id != __uuidof(IDxcIncludeHandler))
                    return E_NOINTERFACE;
                *result = static_cast<IDxcIncludeHandler*>(this);
                AddRef();
                return S_OK;
            }
            ULONG STDMETHODCALLTYPE AddRef() override { return ++m_references; }
            ULONG STDMETHODCALLTYPE Release() override
            {
                const auto remaining = --m_references;
                if(!remaining)
                    delete this;
                return remaining;
            }
            HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR name, IDxcBlob** output) override
            {
                if(!output)
                    return E_POINTER;
                *output = nullptr;
                try {
                    // DXC resolves relative paths and skips inactive directives before calling us.
                    const auto path = std::filesystem::path(name).lexically_normal().generic_u8string();
                    std::string source;
                    if(!read({ reinterpret_cast<const char*>(path.data()), path.size() }, source))
                        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
                    if(source.size() > (std::numeric_limits<UINT32>::max)())
                        return E_OUTOFMEMORY;
                    ComPtr<IDxcBlobEncoding> blob;
                    const auto status = m_utils->CreateBlob(source.data(), static_cast<UINT32>(source.size()), DXC_CP_UTF8, &blob);
                    if(FAILED(status))
                        return status;
                    *output = blob.Detach();
                    return S_OK;
                } catch(...) {
                    return E_FAIL;
                }
            }

        private:
            std::atomic<ULONG> m_references{ 1 };
            ComPtr<IDxcUtils> m_utils;
        };
    }

    std::string readShaderSource(std::string_view path)
    {
        std::string source;
        if(!read(path, source))
            throw std::runtime_error("Cannot read post-effect shader " + std::string(path));
        return source;
    }

    std::string expandShaderIncludes(std::string_view source, std::string_view name)
    {
        ComPtr<IDxcUtils> utils;
        ComPtr<IDxcCompiler3> compiler;
        if(FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))) || FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))))
            throw std::runtime_error("Create DXC preprocessor for " + std::string(name));
        ComPtr<IDxcIncludeHandler> includes;
        includes.Attach(new Includes(utils.Get()));
        const auto source_name = std::filesystem::u8path(name.empty() ? "memory.hlsl" : name).wstring();
        const wchar_t* arguments[]{ source_name.c_str(), L"-P", L"-E", L"main", L"-T", L"ps_6_0", L"-I", L".", L"-HV", L"2016" };
        DxcBuffer input{ source.data(), source.size(), DXC_CP_UTF8 };
        ComPtr<IDxcResult> result;
        if(FAILED(compiler->Compile(&input, arguments, static_cast<UINT32>(std::size(arguments)), includes.Get(), IID_PPV_ARGS(&result))))
            throw std::runtime_error("Preprocess invocation failed for " + std::string(name));
        ComPtr<IDxcBlobUtf8> errors;
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        HRESULT status{};
        // DXC also diagnoses unguarded recursive includes at its nesting limit.
        if(FAILED(result->GetStatus(&status)) || FAILED(status))
            throw std::runtime_error("Preprocess " + std::string(name) + ": " + (errors ? errors->GetStringPointer() : "unknown DXC error"));
        ComPtr<IDxcBlobUtf8> expanded;
        if(FAILED(result->GetOutput(DXC_OUT_HLSL, IID_PPV_ARGS(&expanded), nullptr)) || !expanded)
            throw std::runtime_error("Read preprocessed shader " + std::string(name));
        return { expanded->GetStringPointer(), expanded->GetStringLength() };
    }
}
