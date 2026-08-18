# VTSFloat_Meow

VTSFloat_Meow 是一个将 VTube Studio 的透明 Spout2 画面叠加到桌面的原生 Windows 工具。

当前版本：**Beta 1.0.0**（测试版）

基于 C++/Win32、Direct3D 11 和 Spout2 构建，不注入游戏，也不依赖 Python 或 Qt。
支持透明叠加、GPU 匹配、帧率与比例控制、多屏切换、边框个性化、模型透明度、调试信息和系统托盘。

使用前请在 VTube Studio 中开启 Spout2，并尽量让两者使用同一块 GPU。

当前默认渲染器是原生 C++ 分层窗口。它不使用 Qt、OpenGL、DirectComposition、
DXGI SwapChain 或 `DwmFlush`，也不向游戏注入代码。渲染链路为：

```text
VTube Studio Spout（自动识别发送端 GPU）
  -> D3D11 接收 + CPU staging
  -> 预乘 BGRA 的常驻 DIB
  -> UpdateLayeredWindow
```

这个路径绕开了无上限帧游戏会阻塞数秒的 DXGI `Present`。在《燕云十六声》
无上限帧、游戏位于前台时，实测接收和上屏均为 60 FPS，`UpdateLayeredWindow`
平均约 0.25 ms，没有再出现旧版本的 1–10 秒阻塞。

## 使用

先在 VTube Studio 中开启 Spout，然后双击：

```text
start_overlay.cmd
```

每次启动都会恢复上一次保存的窗口位置和大小，并默认以解锁编辑状态显示完整 GUI；
“完成”造成的锁定和 GUI 隐藏只在当前运行期间有效。

停止覆盖层：

```text
stop_overlay.cmd
```

也可以直接双击根目录的 `VTSFloat_Meow.exe`，或在终端运行：

```powershell
.\VTSFloat_Meow.exe           # 启动；重复运行不会产生第二个窗口
.\VTSFloat_Meow.exe --stop    # 正常关闭原生窗口
```

- 解锁后会在蓝框上方显示原生工具栏，并显示当前全局快捷键。
- 工具栏会跟随模型窗口宽度自动排版：宽窗口使用单排按钮，小窗口切换为双排按钮和双行性能信息，不再超出屏幕或裁掉左侧 GPU 选项。
- `主屏居中`：显示并解锁窗口，恢复为 1280×720，然后移动到主屏工作区中心。
- `切换屏幕`：在所有已启用显示器之间循环移动并居中；只有一个显示器时保持在当前屏幕。
- `锁定/解锁 ...`：默认快捷键为 `Ctrl+Shift+L`；点击后可直接按新的组合键，必须包含 `Ctrl`、`Alt`、`Shift` 或 `Win`；按 `Esc` 取消。
- 性能信息：解锁编辑时始终显示实时 FPS、接收/缩放/上屏耗时、Spout GPU 和当前 GPU。
- 模型缩放：窗口尺寸与 Spout 输出不一致时，使用当前 Spout GPU 的 Direct3D 11 采样来处理模型边缘；原尺寸显示不额外缩放，也不会增加开销。
- `画质`：可选择 `性能`（GPU 最近邻，最低开销）、`平衡`（GPU 双线性，默认推荐）或 `质量`（GPU 双三次，边缘更细腻）。该选择会自动保存。
- `调试`：只控制点击“完成”后的解锁入口。开启时锁定后保留小型“解锁”按钮；关闭时工具栏完全隐藏，只能按全局快捷键重新打开。
- `FPS`：默认选择“跟随窗口所在显示器”，读取 Windows 当前显示模式的固定标称刷新率；窗口移到其他显示器后会自动切换，例如主屏 160 FPS、副屏 60 FPS。它不读取 G-SYNC/FreeSync 的动态刷新率。也可固定为 30/45/60/90/120/144/165/240，或自定义输入 1–240。
- `比例`：`锁定`时缩放始终保持 Spout 纵横比；VTube Studio 拖动后若发送画布比例改变，脚本会自动同步新比例。`自由`时可以单独拖动任意边框进行横向或纵向拉伸。
- 模型悬停透明化：在 `个性化` 中可启用并设置 0–100% 的悬停不透明度。锁定后鼠标经过模型可见区域时会在约 180ms 内平滑透明化，离开后恢复；调整滑块时立即预览。
- 个性化预览期间只透明化模型像素，边框保持完整可见；工具栏“—”左侧预留 GitHub 图标按钮，仓库地址配置后可点击打开。
- `GPU`：列出系统中所有硬件显卡，并标出 Windows 的“节能/高性能”GPU。默认的“自动”会强制跟随 VTube Studio 的 Spout 发送端，也是推荐设置。
- `完成`：隐藏蓝框并恢复点击穿透；调试开启时工具栏缩成小型 `解锁` 按钮，调试关闭时不留下任何按钮。
- 工具栏的 `—` 会把窗口暂时隐藏到系统托盘，不退出程序；可从托盘菜单或双击托盘图标恢复。`×` 仍然表示退出程序。
- 托盘或工具栏执行 `主屏居中` 后会恢复 1280×720，并自动关闭比例锁定，之后可以自由拉伸。
- 解锁后可以拖动窗口或从边缘缩放；拖拽期间保持 60 Hz 实时刷新，纵横比始终跟随 Spout 源。
- `个性化`：模式下拉框包含“自定义 / 跑马灯渐变 / 普通渐变”。普通渐变让整圈同步进行彩虹过渡，跑马灯让彩虹沿周长流动；选择“自定义”时才会展开圆形色盘和亮度滑块，拖动后可实时改变整圈边框颜色，亮度为 0% 时可以选择纯黑。粗细可设为 1–10，默认第 4 档。
- 普通锁定后边框会和编辑控件一起隐藏；调试模式锁定后会保留边框和小型“解锁”入口，方便观察边界。
- 工具栏右侧的 `×` 可以退出；蓝框内部不再放置重复的关闭按钮。
- 程序常驻系统托盘：右键可重置到主屏中央（同时解锁并恢复为 1280×720）、暂时隐藏/显示或退出；双击托盘图标可快速隐藏/显示。隐藏状态不会保存，下次启动仍会正常显示。
- 位置、大小、比例锁定、目标帧率、边框模式/自定义颜色/粗细、快捷键、调试开关和 GPU 选择保存在 `%LOCALAPPDATA%\vts_overlay_layered.ini`，下次自动恢复。
- 性能日志位于 `%LOCALAPPDATA%\vts_overlay_layered.log`。

Spout2 接收端已经作为 C++ 源码直接编译进 `VTSFloat_Meow.exe`，用户不需要另外安装
Spout，也不是启动脚本在磁盘上搜索 Spout。程序运行时会通过 Spout2 查找名为
`VTubeStudioSpout` 的发送端；VTube Studio 负责渲染模型并发送共享纹理，本程序负责接收和显示。

Spout 的共享纹理只能在发送端所在的同一块 GPU 上直接打开。因此选择另一块 GPU 时会先
弹出确认框；选择“否”不会修改任何设置，选择“是”则自动写入 VTube Studio 的 Windows
显卡偏好、正常关闭 VTS、运行 `start_without_steam.bat`，最后自动重启覆盖层。切换期间面捕
会短暂中断。程序优先从正在运行的 VTS 定位目录，也会扫描 Steam 库和默认安装目录。
只有一块 GPU 的电脑也可以正常使用，“自动”会直接选择唯一的显卡。

程序已经不依赖 Python。`start_overlay.cmd` 和 `stop_overlay.cmd` 只是调用根目录原生
EXE 的便捷入口；旧的 Game Bar 小组件和历史参考目录已移除，不参与当前运行链路。

## 构建

```powershell
.\native\build_native.ps1 -Configuration Release
```

输出文件：根目录的 `VTSFloat_Meow.exe`。

关键源码：

```text
VTSFloat_Meow.exe               可直接运行的原生程序
native/overlay_layered.cpp      Spout + UpdateLayeredWindow 渲染器及界面
native/CMakeLists.txt           CMake 构建配置
native/github_mark.ico          GitHub 官方 Invertocat 图标资源
native/overlay_dx11.cpp         已停用的 DXGI/DirectComposition 对照实现
```
