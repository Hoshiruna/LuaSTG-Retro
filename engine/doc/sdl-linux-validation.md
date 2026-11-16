# SDL Linux 兼容性验收

阶段 3 的构建、CPU 测试、GPU 回归及发行包运行结果均待记录。本文列出已有场景、操作与预期结果。

## 执行环境记录

每次执行时填写以下信息，将截图和日志保存在对应记录旁。

| 项目 | 记录 |
| --- | --- |
| 提交版本与执行日期 | 待填写 |
| Linux 发行版、版本、内核及 glibc | 待填写 |
| 编译器、CMake 与 libstdc++ 版本 | 待填写 |
| GPU 型号与驱动版本 | 待填写 |
| 显示会话及缩放比例 | X11 / Wayland，待填写 |
| SDL GPU 实际驱动 | 从 `engine.log` 记录 Vulkan 驱动信息 |
| Windows 对照版本与 GPU 驱动 | 待填写 |
| 发行包文件名与来源 | 待填写 |

## CPU 测试

从仓库根目录执行。

```sh
cmake --preset linux-x64-ci
cmake --build --preset linux-x64-ci
ctest --preset linux-x64-ci
```

`luastg-cpu-tests` 复用引用计数、SDL 输入、文件系统、图像解码、HTTP、shader 反射、几何数据及恢复策略测试。图像测试的资源复制到构建目录。HTTP 测试使用已有的本地服务器脚本。

Linux shader 测试使用 Vulkan 编译器路径。DXIL 翻译专项在 Windows 构建中执行。`Core.Audio.Test` 需要设备交互，GPU capture 需要图形设备，它们通过下文的目标机操作验收。

| 项目 | 结果与证据 |
| --- | --- |
| Linux 核心 runtime 构建 | 未执行 |
| CPU 测试及失败日志 | 未执行 |
| Windows D3D11 回归 | 未执行 |
| Windows SDL GPU 回归 | 未执行 |

CI 配置位于 `.github/workflows/linux.yml`。CPU 测试成功后，工作流会使用关闭测试的 Release 预设构建发行候选包。实际 GPU、声音输出和桌面操作的结果在本记录中另行填写。

## 游戏场景

在带 Vulkan 驱动、桌面会话和音频设备的 Linux x64 目标机上执行。先按 `BUILD.md` 安装字体，再从仓库根目录运行。

```sh
cd data/test
export LUASTG_TEST_FONT=/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc
../../build/linux-x64-ci/bin/LuastgRetro
```

在 `Select Test` 窗口选择对应场景。配置中的 `renderer_driver` 使用 `vulkan` 时可固定后端，使用 `auto` 时应核对日志中的实际选择。

| 项目 | 现有场景或操作 | 预期结果 | 结果 |
| --- | --- | --- | --- |
| 2D 渲染 | `Graphics: deterministic core 2D` | 对照 Windows D3D11 和 Windows SDL GPU，检查混合、透明度、采样、裁剪、坐标方向及嵌套渲染目标 | 未执行 |
| 后处理 | `Graphics: deterministic post-effects` | 参数布局、纹理绑定、采样偏移及目标切换与对照版本一致 | 未执行 |
| 资源加载 | `Graphics: shader archive / async loading (SDL GPU)` | 归档与异步加载完成，错误 shader 有诊断，取消及销毁资源池后无残留任务 | 未执行 |
| 几何 | `Graphics: deterministic meshes and models` | Mesh 顶点布局、深度、模型方向与材质对照一致 | 未执行 |
| 字体及窗口 | `test_window_and_display.lua` 注册的场景 | 从 `LUASTG_TEST_FONT` 加载字体，检查文字边界与窗口尺寸 | 未执行 |
| 输入 | `Input: SDL3`，键鼠及手柄连接、断开 | 按键状态、鼠标位置和设备列表随操作更新 | 未执行 |
| 音频 | `test/audio/SoundEffect.lua`，现有音乐场景 | 声音可听，循环、暂停和恢复正常，重复加载释放后可继续播放 | 未执行 |
| 窗口生命周期 | 缩放、最小化、恢复、失焦、全屏切换 | 恢复显示后继续更新，退出后进程结束 | 未执行 |
| 高 DPI | 在不同缩放比例及显示器间移动窗口 | 渲染尺寸与鼠标坐标对应，UI 可正常操作 | 未执行 |
| UTF-8 路径 | 将游戏及素材置于包含中文和空格的路径 | 脚本、图片、字体与音频均能加载 | 未执行 |
| 错误报告 | 缺失资源、语法错误 shader、无效 renderer_driver | 返回失败并给出包含资源或驱动信息的诊断 | 未执行 |

2D 场景输出 `2d-capture.png`、`2d-target.png`，后处理场景输出 `posteffect-capture.png`、`posteffect-target.png`。保留各平台原始截图，逐项记录像素差异。文字使用相同字体文件和字号对照。出现差异时记录区域、输入和日志，待定位后修复。

## GPU 恢复

以下工具复用现有故障注入与资源重建逻辑。每次运行一个场景，并保留该次运行的输出。

```sh
cmake --build --preset linux-x64-gpu-tools
mkdir -p build/linux-x64-ci/recovery-acquisition
cd build/linux-x64-ci/recovery-acquisition
../engine/graphics/sdlgpu/Core.Graphics.SDLGPU.RecoveryCapture acquisition
```

将最后一个参数依次换为 `submission`、`upload`、`second`、`reconstruction`，每次使用单独的输出目录。

| 场景 | 预期行为 | 结果 |
| --- | --- | --- |
| acquisition / submission / upload | 首次故障后恢复，纹理、模型、shader 与渲染目标重建，恢复前后画面可对照 | 未执行 |
| second | 首次恢复成功，第二次故障触发 Fatal 状态，工具返回非零状态 | 未执行 |
| reconstruction | 重建失败进入 Fatal 状态，日志说明失败原因 | 未执行 |

同时核对 `recovery-before.png`、`recovery-after.png` 及 `recovery-target-*.png`。预期失败场景的非零退出需要结合诊断判断。

## 发行候选包

按 `BUILD.md` 生成 `.tar.gz` 后，将包复制到独立于开发目录的 Linux 目标环境。目标环境需要 Vulkan 驱动、桌面和音频运行库，以及与构建基线兼容的 glibc 和 libstdc++。

| 操作 | 预期结果 | 结果 |
| --- | --- | --- |
| 解压后执行 `run-luastg` | 示例显示文字、玩家与弹幕，键盘可控制玩家 | 未执行 |
| 从其他工作目录通过绝对路径执行 launcher | 示例资源仍能加载 | 未执行 |
| 将解压目录改为包含中文和空格的名称后启动 | 库、字体与资源正常加载 | 未执行 |
| 在未设置开发用 `LD_LIBRARY_PATH` 的环境启动 | 从包内找到 DXC 及随包共享库 | 未执行 |
| 检查 `licenses/` 与字体资源 | 包含项目、构建依赖、OpenSSL 及 Noto CJK 的许可证文本 | 未执行 |
| 正常退出后再次启动 | 配置、日志与用户目录正常工作 | 未执行 |

CPU 测试通过、GPU 兼容性对照完成、恢复场景符合预期，并取得独立环境的发行包启动记录后，填写阶段 3 的验收结论。
