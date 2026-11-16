#include "Core/Graphics/Common/SystemFont.hpp"
#include "utf8.hpp"
#include <Windows.h>
#include <cwchar>
#include <ShlObj.h>

namespace core::Graphics::Common
{
    bool findSystemFont(std::string_view const name, std::string& u8_path)
    {
        std::wstring wide_name(utf8::to_wstring(name));

        // 打开注册表 HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts
        // 枚举符合要求的字体
        HKEY tKey = NULL;
        if(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_READ, &tKey) == ERROR_SUCCESS) {
            // 枚举子键
            int tIndex = 0;
            WCHAR tKeyName[MAX_PATH]{};
            DWORD tKeyNameLen = MAX_PATH;
            DWORD tKeyType = 0;
            BYTE tKeyData[MAX_PATH]{};
            DWORD tKeyDataLen = MAX_PATH;

            while(RegEnumValueW(tKey, tIndex, tKeyName, &tKeyNameLen, NULL, &tKeyType, tKeyData, &tKeyDataLen) == ERROR_SUCCESS) {
                // 检查是否为相应字体
                if(tKeyType == REG_SZ) {
                    WCHAR tFontName[MAX_PATH]{};
                    WCHAR tFontType[MAX_PATH]{};
                    if(2 == swscanf_s(tKeyName, L"%[^()] (%[^()])", tFontName, MAX_PATH, tFontType, MAX_PATH)) {
                        size_t tLen = wcslen(tFontName);

                        // 去除scanf匹配的空格
                        if(!tLen)
                            continue;
                        else {
                            if(tFontName[tLen - 1] == L' ')
                                tFontName[tLen - 1] = L'\0';
                        }
                        // 是否为需要的字体
                        if(tFontName == wide_name) {
                            RegCloseKey(tKey);

                            WCHAR tTextDir[MAX_PATH]{};
                            SHGetSpecialFolderPathW(GetDesktopWindow(), tTextDir, CSIDL_FONTS, 0);
                            std::wstring tPath = tTextDir;
                            tPath += L'\\';
                            tPath += (WCHAR*)tKeyData;

                            // 返回路径
                            u8_path = utf8::to_string(tPath);
                            return true;
                        }
                    }
                }

                // 继续枚举
                tKeyNameLen = MAX_PATH;
                tKeyType = 0;
                tKeyDataLen = MAX_PATH;
                tIndex++;
            }
        } else {
            return false; // 打开注册表失败
        }

        RegCloseKey(tKey);

        return false; // 没找到
    }
}
