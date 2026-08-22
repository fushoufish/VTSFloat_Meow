<p align="center">
  <img src="VTSFloat_Meow.png" alt="VTSFloat_Meow Logo" width="240">
</p>

<h1 align="center">VTSFloat_Meow</h1>

<p align="center">将 VTube Studio 的透明模型直接悬浮到桌面、游戏或其他应用上方。</p>

- VTSFloat_Meow 是一个面向 Windows10/11 的 VTube Studio 透明模型悬浮层。当前版本：**Beta 1.0.3**。
- (暂未在windows10中进行实验，理论上讲推测预计估计应当推断大概率极其可能是没问题的....吧) (=^･ω･^=)

## 1. 这个工具解决什么问题

许多用户( ~~包括我~~ )希望把 VTube Studio 模型放到游戏、桌面或其他应用上方然后美美的( ~~傻傻的~~ )欣赏自己的模型动态。

传统做法通常要借助 OBS、Bandicam 等工具完成采集、抠图、透明图层和窗口叠加，配置复杂，也会引入额外的编码、合成显示等性能开销。
然后就诞生了这个项目来解决这个问题。

VTSFloat_Meow 的实现原理：VTube Studio 负责渲染模型并通过 Spout2 发送共享纹理，本程序负责接收、缩放、合成透明画面并显示。它不向游戏、应用或系统底层注入代码，也不依赖 OBS、Bandicam 或 Python。
Spout2是什么？→是Windows上用于在不同图形程序之间共享GPU纹理的技术。画面可以直接在显存中跨程序传递，避免传统的视频编码和解码
## 2. 主要功能

- 任意调整模型窗口大小，并支持比例锁定或自由拉伸。
- 主屏居中、多显示器切换、恢复上次窗口位置和大小。
- 支持全屏游戏、无边框窗口和普通桌面应用上层显示。
- 锁定后鼠标可穿透模型，与下方应用/游戏界面继续交互。
- 可设置鼠标经过模型时的不透明度和触发范围，且平滑过渡。
- 可在悬停时触发表情，离开或超时后恢复原先的表情状态。
- 自定义边框颜色、渐变模式、粗细、调试信息和帧率策略。
- 自动匹配 VTube Studio 的 Spout GPU，并提供多 GPU 选择和 VTube Studio 启动辅助。
- 系统托盘、快捷键、VTube Studio Plugins API 连接和设置缓存。

## 3. 工作原理

VTube Studio 负责渲染模型并通过 Spout2 发送 GPU 共享纹理；VTSFloat_Meow 负责接收、缩放、合成透明画面，并使用原生 Win32 分层窗口显示。程序不注入游戏或应用，也不读取游戏画面。

完整的渲染管线、数据流、GPU/帧率同步和性能统计说明，请参阅：[项目说明：实现原理、渲染管线与性能演进](./md/项目说明_实现原理与性能演进.md)。

## 4. 使用须知

### 1. 基本使用

1. 启动 VTube Studio。
2. 在 VTube Studio 的 设置 → 相机”中启用 **Spout2**，背景选择 `ColorPicker` 并打开 `透明推流 ( 关键 )`。
3. 启动 `VTSFloat_Meow.exe` 或 `start_overlay.cmd`

4. 首次连接 VTube Studio Plugins API 时，在 VTS 中允许插件访问。
5. 点击“完成”锁定覆盖层；默认快捷键为 `Ctrl+Shift+L`。

### 2. VTube Studio API 开启教程

1. 在 VTube Studio 的 设置 → 找到 **VTube Studio Plugins / Plugin API** 区域。
2. 打开 **Start API** 或 **Allow Plugin API access**。
3. 端口保持默认的 `8001` 即可。若端口被占用，VTube Studio 可能会自动使用 `8002` 或其他端口。
4. 保持 VTube Studio 和模型运行，然后启动 `VTSFloat_Meow.exe`。
5. 第一次连接时，VTube Studio 会弹出插件授权提示，请点击 **允许 / Allow**。
6. 授权成功后，脚本即可使用 API 读取模型状态、获取统计信息和触发表情。之后再次启动通常不会重复弹出授权窗口。

如果没有出现授权提示，请确认 API 已开启、VTube Studio 没有运行多个实例，并检查 API 设置中显示的实际端口。

# ***为什么推荐核显/节能 GPU？

Spout2 共享纹理通常只能在发送端所在的 GPU 上直接打开。VTube Studio、Spout2 和 VTSFloat_Meow 最好运行在同一块 GPU 上。推荐使用核显或节能 GPU，是因为高性能独显通常还要承担游戏或高负载应用的渲染任务。

当游戏以无上限帧率运行时，独显的渲染队列、显存带宽和线程调度可能已经接近满载。此时再让 VTube Studio 和覆盖层争用同一块 GPU，可能导致 Spout 互斥等待、帧率下降，严重时表现为视频、游戏或覆盖层短暂卡死。使用核显可以把模型链路与游戏渲染分开。

在无限暖暖,异环(虚幻5)，燕云十六声(Messiah)，CS2(起源2)中 将VTSFloat_Meow,Spout2,VTube Studio 运行在同游戏的高性能独显时 几乎无法正常渲染，除非手动降低游戏画质与帧率上限）

只有一块 GPU 的电脑也可以使用；程序会直接使用这块 GPU。多 GPU 电脑切换 GPU 后，需要按提示重启 VTube Studio 和覆盖层。

### INI 缓存

设置保存在：

```text
%LOCALAPPDATA%\vts_overlay_layered.ini
```

其中包括窗口位置和大小、比例锁定、帧率、边框、快捷键、GPU 选择及 VTube Studio API 端口。删除该文件，或在“调试 → 清除缓存并重置脚本”中操作，可恢复默认设置。

## 5. 性能与性能设计

### 分辨率匹配

建议 VTube Studio 画布分辨率与 VTSFloat_Meow 窗口分辨率尽量接近。两者分辨率越大，接收、缩放、显存读写和分层窗口合成的成本越高。

- **VTS 分辨率大于覆盖层窗口**：建议先把 VTS 画布缩小到接近覆盖层大小。这样画质通常已经足够，除非使用 4K 或更高分辨率屏幕，否则更高源分辨率往往只是额外消耗性能。
- **VTS 分辨率小于覆盖层窗口**：覆盖层需要放大画面，整体性能压力通常更低，但细节会受源分辨率限制。程序提供 GPU 双线性和双三次采样，平衡模式可以减少放大后的锋利锯齿。

### 调试信息怎么判定

调试模式中的 `FPS` 是覆盖层实际完成并上屏的帧率；`目标` 是覆盖层请求的刷新率；`VTS配置` 来自 VTube Studio 配置文件；`VTS实时` 来自 VTube Studio API 统计。`接收/缩放/上屏` 分别是 Spout 接收、缩放转换和 `UpdateLayeredWindow` 的平均耗时。

调试栏中的“对齐”不是简单比较两个 FPS，而是比较 VTS 源画布和覆盖层窗口的像素面积：

```text
对齐率 = min(VTS像素数, 覆盖层像素数) / max(VTS像素数, 覆盖层像素数) × 100%
```

### 帧率设置

优先选择“跟随 VTS 配置（推荐）”或使用不高于 VTS 实际输出的固定帧率。覆盖层设置得比 VTS 更高不会产生更多模型帧，只会重复接收、缩放和上屏，形成无用功，并可能增加 GPU 调度压力。VSYNC、VSYNC_HALF 等模式也会限制发送端更新频率。

## 6. 技术栈

| 模块 | 当前实现 | 作用 |
| --- | --- | --- |
| 语言 | C++17 | 主程序和渲染循环 |
| 窗口 | Win32 分层窗口（`WS_EX_LAYERED`） | 显示透明像素、置顶、鼠标穿透 |
| 图形设备 | Direct3D 11 / DXGI | 创建 GPU 设备、读取显卡和显示器信息 |
| 画面传输 | 项目内置 Spout2 DirectX 接收端 | 从 VTS 取共享纹理，不经过网络和磁盘 |
| 缩放 | D3D11 GPU shader（性能/平衡/质量三档） | 在显卡上完成模型缩放和滤波 |
| 最终上屏 | 32 位 BGRA DIB + `UpdateLayeredWindow` | 把透明画面交给 Windows 桌面合成器 |
| 性能统计 | Windows PDH GPU Engine + DXGI 显存查询 | 显示本程序 GPU 占用、GPU 内存和帧耗时 |
| 设置 | `%LOCALAPPDATA%\vts_overlay_layered.ini` | 保存位置、尺寸、帧率、GPU、外观等设置 |
| 构建 | CMake + MSVC，脚本为 `native/build_native.ps1` | 生成根目录的 `VTSFloat_Meow.exe` |

## 7. 早期瓶颈与解决方向

项目早期的失败路线、性能瓶颈、问题定位过程和当前解决方向已集中整理到：[项目说明：实现原理、渲染管线与性能演进](./md/项目说明_实现原理与性能演进.md)。

## 8. 使用前提

- Windows 10/11，建议使用支持 Direct3D 11 的硬件 GPU。
- 已安装并正常运行 VTube Studio。
- VTube Studio 已开启 Spout2 透明推流，发送端名称为 `VTubeStudioSpout`。
- VTube Studio、Spout2 发送端与 VTSFloat_Meow 尽量使用同一块 GPU。
- 若使用表情悬停功能，需要在 VTube Studio 中授权 VTSFloat_Meow Plugins API。

## 构建

安装 Visual Studio 2022 C++ 工具链，在项目根目录执行：

```powershell
.\native\build_native.ps1 -Configuration Release
```

输出为根目录的 `VTSFloat_Meow.exe`。Spout2 接收端已编译进程序，用户不需要另行安装 Spout2。

## 目录说明

```text
VTSFloat_Meow.exe           可直接运行的程序
native/                     C++ 源码、CMake 配置和编译脚本
native/third_party/Spout2   内置 Spout2 源码
start_overlay.cmd           启动程序的便捷脚本
stop_overlay.cmd            关闭程序的便捷脚本
md/                         面向开发者/更多了解的补充说明
```

## 许可证与发布状态

任处于测试开发阶段
