# 构建 LuaSTG Retro

## 当前目标

完整 runtime 的构建入口面向 Windows x64/MSVC 与 Linux x64/SDL GPU。Linux 的实际编译、兼容性与发行包运行结果待记录，详见 [SDL Linux 兼容性验收](engine/doc/sdl-linux-validation.md)。macOS 移植安排在后续阶段。

基础工具为 Git、CMake 3.31 或更新版本和支持 C++20 的编译器。测试及部分工具还使用 C++23 标准库。Linux CI 配置选用 Ubuntu 24.04、GCC 14 和 Ninja。Visual Studio 2026 生成器要求 CMake 4.2 或更新版本。

取得完整源码与测试素材后再配置项目。

```sh
git submodule update --init --recursive
git lfs pull
```

依赖源码通过 CPM 获取，缓存位于 `build/packages`。DXC 使用项目固定的发行版本，首次配置需要下载相应的平台包。

## Windows x64

默认预设使用 Visual Studio 2026。Debug 与 Release 构建命令如下。

```powershell
cmake --preset vs2026-amd64
cmake --build --preset windows-vs2026-amd64-debug
cmake --build --preset windows-vs2026-amd64-release
```

Release 工作流预设为 `windows-vs2026-amd64-release`。默认图形后端为 D3D11，配置时传入 `-DLUASTG_GRAPHICS_BACKEND=SDLGPU` 可选择 SDL GPU。

使用 Visual Studio 2022 时，在根目录 `CMakeUserPresets.json` 中启用相应预设。

```json
{
  "version": 6,
  "include": ["cmake/presets/vs2022.json"]
}
```

随后执行。

```powershell
cmake --preset vs2022-amd64
cmake --build --preset windows-amd64-release
```

其他预设保留在 `cmake/presets`。Windows Ninja 预设通过 `ninja.json` 启用，需要在 AMD64 Visual Studio 开发者终端中使用。当前完整 runtime 限制为 x64，目录中的旧 x86、ARM64 预设仍需后续迁移。

## Linux x64

Ubuntu 24.04 的构建依赖与 CI 配置一致。

```sh
sudo apt-get update
sudo apt-get install -y gcc-14 g++-14 ninja-build nasm pkg-config python3-venv \
  libssl-dev libvulkan-dev libx11-dev libxext-dev libxrandr-dev \
  libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev \
  libwayland-dev libxkbcommon-dev libdecor-0-dev libasound2-dev \
  libpulse-dev libudev-dev libdrm-dev libgbm-dev libegl1-mesa-dev \
  libgl1-mesa-dev fonts-noto-cjk
```

若系统 CMake 低于 3.31，可在虚拟环境中安装 3.x 版本。

```sh
python3 -m venv build/venv
. build/venv/bin/activate
python -m pip install 'cmake>=3.31,<4'
```

选择编译器后，从仓库根目录配置并构建。

```sh
export CC=gcc-14
export CXX=g++-14
cmake --preset linux-x64-release
cmake --build --preset linux-x64-release
```

Debug 使用 `linux-x64-debug`。可执行文件与构建时复制的 DXC 位于对应构建目录的 `bin/` 中。

Linux 预设选择 SDL GPU，并关闭 Steam、Discord 集成。当前视频后端不可用，调用会返回失败并记录原因。系统字体名称查找尚待迁移，游戏应提供字体文件路径。平台专属 Lua API 的不可用功能会明确报错。

## CPU 测试与 GPU 回归

以下命令会实际构建并执行测试，按需要手动运行。

```sh
cmake --preset linux-x64-ci
cmake --build --preset linux-x64-ci
ctest --preset linux-x64-ci
```

`linux-x64-ci` 使用独立构建目录并打开测试。构建预设包含 runtime 与 `luastg-cpu-tests`，CTest 按 `cpu` 标签选择已有测试。HTTP 集成测试需要 Python 3。

图像测试使用构建目录内的素材副本。SDL GPU 的 CPU 测试覆盖 HLSL 编译、反射、几何数据及恢复策略。需要图形设备的 capture 工具和音频交互场景在目标机上运行，步骤及记录表见 [SDL Linux 兼容性验收](engine/doc/sdl-linux-validation.md)。

Linux CI 工作流定义在 `.github/workflows/linux.yml`，包含核心构建、CPU 测试、独立 Release 构建与发行候选包归档。

## Linux 目录式发行包

使用关闭测试的 `linux-x64-release` 构建发行包，可避免将故障注入界面带入发行 runtime。构建成功后执行。

```sh
cpack --config build/linux-x64-release/CPackConfig.cmake
```

归档输出到 `build/linux-x64-release/releases/`，文件名使用 `version.json` 中的版本号。也可以将同一组件安装到指定目录。

```sh
cmake --install build/linux-x64-release --component LinuxRuntime \
  --prefix "$PWD/build/linux-x64-release/stage"
```

包内包含 runtime、DXC、收集到的其他共享库、示例资源、Noto CJK 字体与许可证。安装后的 runtime 通过 `$ORIGIN` 查找随包库，`run-luastg` 会先切换到发行包目录，再传递参数启动程序。

```sh
cd build/linux-x64-release/stage
./run-luastg
```

glibc、libstdc++ 和 GCC 基础运行库由目标系统提供。目标环境应兼容构建基线，并安装 Vulkan loader、对应 GPU 驱动及桌面和音频运行库。使用 GCC 14 构建时，目标机也需要兼容的 libstdc++。

打包时使用已安装的 `NotoSansCJK-Regular.ttc` 和对应许可证。其他安装布局可以在配置时指定以下路径。

```sh
cmake --preset linux-x64-release \
  -DLUASTG_NOTO_CJK_FONT=/absolute/path/NotoSansCJK-Regular.ttc \
  -DLUASTG_NOTO_CJK_LICENSE=/absolute/path/font-license \
  -DLUASTG_OPENSSL_LICENSE=/absolute/path/openssl-license
```

`LinuxRuntime` 组件在缺少这些打包素材时报告具体缺项。安装依赖后重新配置即可更新查找结果。

## Windows Steam 配置

将 Steamworks SDK 放入 `external/steam_api/SteamworksSDK/sdk`。设置 `LUASTG_STEAM_API_ENABLE=TRUE` 和 `LUASTG_STEAM_API_APP_ID` 后重新配置。需要从 Steam 启动时，再开启 `LUASTG_STEAM_API_FORCE_LAUNCH_BY_STEAM`。

`cmake/example/CMakeUserPresets.SteamAPI.json` 提供自定义预设的参考。预设名称应以所包含的 Visual Studio 预设文件为准。
