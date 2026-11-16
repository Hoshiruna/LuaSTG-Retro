#include "ShaderSource.hpp"
#include "DxcObject.hpp"
#include "core/FileSystem.hpp"
#include "core/SmartReference.hpp"
#include "utf8.hpp"
#include <atomic>
#include <filesystem>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace core::Graphics::SDLGPU
{
    namespace
    {
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
            explicit Includes(IDxcUtils* utils) : m_utils(utils) { utils->AddRef(); }
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
                    // DXC resolves relative paths and skips inactive directives first.
                    const auto path = std::filesystem::u8path(utf8::to_string(name)).lexically_normal().generic_u8string();
                    std::string source;
                    if(!read({ reinterpret_cast<const char*>(path.data()), path.size() }, source))
                        return E_FAIL;
                    if(source.size() > (std::numeric_limits<UINT32>::max)())
                        return E_OUTOFMEMORY;
                    IDxcBlobEncoding* blob{};
                    const auto status = m_utils->CreateBlob(source.data(), static_cast<UINT32>(source.size()), DXC_CP_UTF8, &blob);
                    if(FAILED(status))
                        return status;
                    *output = blob;
                    return S_OK;
                } catch(...) {
                    return E_FAIL;
                }
            }

        private:
            std::atomic<ULONG> m_references{ 1 };
            DxcObject<IDxcUtils> m_utils;
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
        IDxcUtils* raw_utils{};
        if(FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&raw_utils))))
            throw std::runtime_error("Create DXC utilities for " + std::string(name));
        DxcObject<IDxcUtils> utils(raw_utils);
        IDxcCompiler3* raw_compiler{};
        if(FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&raw_compiler))))
            throw std::runtime_error("Create DXC preprocessor for " + std::string(name));
        DxcObject<IDxcCompiler3> compiler(raw_compiler);
        DxcObject<IDxcIncludeHandler> includes(new Includes(utils.get()));
        const auto source_name = utf8::to_wstring(name.empty() ? "memory.hlsl" : name);
        const wchar_t* arguments[]{ source_name.c_str(), L"-P", L"-E", L"main", L"-T", L"ps_6_0", L"-I", L".", L"-HV", L"2016" };
        DxcBuffer input{ source.data(), source.size(), DXC_CP_UTF8 };
        IDxcResult* raw_result{};
        if(FAILED(compiler->Compile(&input, arguments, static_cast<UINT32>(std::size(arguments)), includes.get(), IID_PPV_ARGS(&raw_result))))
            throw std::runtime_error("Preprocess invocation failed for " + std::string(name));
        DxcObject<IDxcResult> result(raw_result);
        IDxcBlobUtf8* raw_errors{};
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&raw_errors), nullptr);
        DxcObject<IDxcBlobUtf8> errors(raw_errors);
        HRESULT status{};
        if(FAILED(result->GetStatus(&status)) || FAILED(status))
            throw std::runtime_error("Preprocess " + std::string(name) + ": " + (errors ? errors->GetStringPointer() : "unknown DXC error"));
        IDxcBlobUtf8* raw_expanded{};
        if(FAILED(result->GetOutput(DXC_OUT_HLSL, IID_PPV_ARGS(&raw_expanded), nullptr)) || !raw_expanded)
            throw std::runtime_error("Read preprocessed shader " + std::string(name));
        DxcObject<IDxcBlobUtf8> expanded(raw_expanded);
        return { expanded->GetStringPointer(), expanded->GetStringLength() };
    }
}
