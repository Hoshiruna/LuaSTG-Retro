# SDL GPU smoke dependency notices

The smoke application is part of LuaSTG Retro and uses its MIT license.
The build copies original notices into the `licenses` directory beside the
executable. Distribute that directory and this file with copied smoke binaries.

## Shader dependencies added for Phase 1

| Dependency | Pinned revision | License / packaged notices |
| --- | --- | --- |
| [SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross) | `1ff05bec573988a98ef9e0260b4da44f512b8367` | zlib; `licenses/SDL_shadercross/LICENSE.txt` |
| [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) | `1a6169566c73d3da552748fc372fe2bbb856e46e` | Apache-2.0; `licenses/SPIRV-Cross/LICENSE` |
| [DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2607) | `v1.9.2607`, `dxc_2026_07_29.zip` | Microsoft's published LLVM and Microsoft notices; `licenses/DXC/LICENSE-LLVM.txt` and `LICENSE-MS.txt` |

The DXC archive is verified against the published SHA-256
`a1dfb116ba3eeae6a1582291b53a8e7bf65ad760676bd3194685c8f7367cd241`.
Only its x64 `dxcompiler.dll` and `dxil.dll` are copied into the smoke output.
Shadercross and SPIRV-Cross are built from their unmodified pinned sources.

## Existing dependencies used by the smoke application

- SDL 3.4.8: zlib license, copied to `licenses/SDL/LICENSE.txt`.
- Dear ImGui and its SDL backends: MIT, copied to `licenses/ImGui/LICENSE.txt`.
- FreeType: distributed under the FreeType License; copied to
  `licenses/FreeType/FTL.TXT`, with the upstream license overview. Portions of
  this software are copyright The FreeType Project (www.freetype.org).
- spdlog 1.17.0: MIT, copied to `licenses/spdlog/LICENSE`. Its bundled fmt 12.1.0
  notice is included in `licenses/fmt/LICENSE`.
- simdutf 9.0.0: distributed under its MIT option, with the original notice in
  `licenses/simdutf/LICENSE-MIT`.
- nlohmann/json 3.12.0: MIT, with the original notice in
  `licenses/nlohmann-json/LICENSE.MIT`.
- stb at `2c980bb59875b0d32144a71867fbdebb2f77cd20`: distributed under its
  MIT option; the original dual-license notice is copied to `licenses/stb/LICENSE`.
  The SDL GPU support library uses `stb_image_write` for PNG encoding.

The checked-in MIT notices come from the corresponding pinned upstream releases:
[simdutf](https://github.com/simdutf/simdutf/blob/v9.0.0/LICENSE-MIT),
[nlohmann/json](https://github.com/nlohmann/json/blob/v3.12.0/LICENSE.MIT), and
[fmt](https://github.com/fmtlib/fmt/blob/12.1.0/LICENSE).

No FFmpeg dependency is introduced in Phase 1.
