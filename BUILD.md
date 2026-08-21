# Building LuaSTG Retro

## Current platform support

The shipping runtime currently builds on Windows 10 1809 or newer for x64. The Linux and macOS presets build the portable window and input modules only. Full runtime builds on those platforms will become available as the remaining platform backends are migrated.

The build requirements are as follows:
CMake 3.31 or newer and Git. 
Windows builds use Visual Studio 2022 with the x64 C++ toolchain. 
Linux and macOS presets use Ninja with the platform's default C and C++ compilers.

## Windows x64

Configure the project:

```shell
cmake --preset windows-x64
```

Build either configuration:

```shell
cmake --build --preset windows-x64-debug
cmake --build --preset windows-x64-release
```

The release workflow runs both steps:

```shell
cmake --workflow --preset windows-x64-release
```

## Linux x64

The current presets configure the portable module boundary and build `Core.WindowSystem` and `Core.InputSystem`:

```bash
cmake --preset linux-x64-debug
cmake --build --preset linux-x64-debug
```

Use `linux-x64-release` for a release configuration.

## macOS universal

The macOS preset targets macOS 12 or newer and produces universal x86_64 and arm64 libraries for the portable modules:

```bash
cmake --preset macos-universal-debug
cmake --build --preset macos-universal-debug
```

Use `macos-universal-release` for a release configuration.

## Steam API support

Steam support is optional. To enable it:

1. Download the Steamworks SDK from the [Steamworks partner site](https://partner.steamgames.com/downloads/list).
2. Copy its `sdk` directory to `external/steam_api/SteamworksSDK`.
3. Set `LUASTG_STEAM_API_ENABLE` to `TRUE` and provide `LUASTG_STEAM_API_APP_ID`.
4. Set `LUASTG_STEAM_API_FORCE_LAUNCH_BY_STEAM` to `TRUE` only when the game must be launched through Steam.

The example at `cmake/example/CMakeUserPresets.SteamAPI.json` contains these settings. Copy it to the repository root as `CMakeUserPresets.json`, change the application ID, and run:

```powershell
cmake --workflow --preset windows-x64-my-steam-api-release
```

# 编译 LuaSTG Retro

## 当前平台支持

完整运行时目前支持 Windows 10 1809 及更高版本，仅支持 x64。Linux 和 macOS 预设目前只编译可移植的窗口与输入模块。其余平台后端完成迁移后，这两个平台才会开放完整运行时编译。

编译要求如下：

需要 CMake 3.31 或更高版本以及 Git。
Windows 使用 Visual Studio 2022 的 x64 C++ 工具链。
Linux 和 macOS 预设使用 Ninja 与系统默认的 C/C++ 编译器。

## Windows x64

配置项目：

```shell
cmake --preset windows-x64
```

编译测试版或发布版：

```shell
cmake --build --preset windows-x64-debug
cmake --build --preset windows-x64-release
```

发布工作流会依次执行配置和编译：

```shell
cmake --workflow --preset windows-x64-release
```

## Linux x64

当前预设用于验证可移植模块边界，并编译 `Core.WindowSystem` 和 `Core.InputSystem`：

```bash
cmake --preset linux-x64-debug
cmake --build --preset linux-x64-debug
```

发布配置请改用 `linux-x64-release`。

## macOS 通用版

macOS 预设的最低系统版本为 macOS 12，可移植模块会生成同时包含 x86_64 和 arm64 的通用库：

```bash
cmake --preset macos-universal-debug
cmake --build --preset macos-universal-debug
```

发布配置请改用 `macos-universal-release`。

## Steam API 支持

Steam 支持是可选功能。启用方法如下：

1. 从 [Steamworks 合作伙伴网站](https://partner.steamgames.com/downloads/list) 下载 Steamworks SDK。
2. 将 SDK 中的 `sdk` 目录复制到 `external/steam_api/SteamworksSDK`。
3. 将 `LUASTG_STEAM_API_ENABLE` 设为 `TRUE`，并填写 `LUASTG_STEAM_API_APP_ID`。
4. 只有在游戏必须通过 Steam 启动时，才将 `LUASTG_STEAM_API_FORCE_LAUNCH_BY_STEAM` 设为 `TRUE`。

`cmake/example/CMakeUserPresets.SteamAPI.json` 提供了示例配置。将它复制到仓库根目录并重命名为 `CMakeUserPresets.json`，修改应用 ID 后执行：

```powershell
cmake --workflow --preset windows-x64-my-steam-api-release
```
