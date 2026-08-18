# VTSFloat_Meow 项目说明

## 1. 项目定位

VTSFloat_Meow 是一个 Windows 原生桌面叠加层，用于接收 VTube Studio 通过 Spout2
发送的透明模型画面，并将其显示在游戏或其他窗口上方。

当前版本：**Beta 1.0.0**。

项目不向游戏注入代码，也不依赖 Python 或 Qt。VTube Studio 负责渲染模型并发送
共享纹理，VTSFloat_Meow 负责接收、缩放、合成透明画面并显示。

## 2. 技术栈

- C++17
- Win32 窗口与输入 API
- Direct3D 11 / Spout2
- `UpdateLayeredWindow` 分层透明窗口
- CMake + Visual Studio 2022

## 3. 画面管线

```text
VTube Studio
    -> Spout2 共享纹理
    -> D3D11 接收（发送端 GPU）
    -> 必要时 GPU 缩放
    -> 预乘 BGRA DIB
    -> UpdateLayeredWindow
    -> 桌面/游戏上方的透明窗口
```

### 3.1 接收

程序查找 VTube Studio 的 Spout 发送端，读取共享纹理尺寸、格式和发送端 GPU。
接收端尽量使用同一块 GPU，避免跨 GPU 共享纹理失败或产生额外复制。

### 3.2 缩放与比例

当窗口尺寸与 Spout 画布不一致时，程序在 GPU 上完成缩放，再将结果复制到常驻的
BGRA 缓冲区。比例锁定时保持源画布比例，自由模式允许横向或纵向单独拉伸。

### 3.3 透明显示

模型画面使用预乘 Alpha，窗口通过 Win32 分层窗口提交到桌面合成器。边框、工具栏
和调试信息属于独立的 UI 图层，不参与模型透明度计算。

## 4. 主要功能

- Spout2 透明画面接收
- GPU 自动匹配与多 GPU 选择
- 固定帧率、跟随显示器刷新率和自定义帧率
- 窗口移动、缩放、多屏切换和比例锁定
- 边框颜色、渐变模式和粗细设置
- 模型透明度与悬停透明化
- 调试性能信息和 FPS 曲线
- 系统托盘、快捷键和位置/设置自动保存
- VTube Studio 启动辅助

## 5. 性能设计

渲染线程只处理接收、缩放、合成和上屏；窗口交互、工具栏和设置使用独立的 UI
路径。常驻缓冲区避免每帧分配大块内存，日志默认不记录高频渲染指标。

性能栏中的接收、缩放、上屏耗时用于定位瓶颈，不代表 VTube Studio 的内部渲染
帧率。VTube Studio 的 VSYNC 或帧率设置会限制发送端更新频率。

## 6. 早期瓶颈与解决方向

### Python/Qt 主线程排队

旧方案将接收、绘制和窗口消息集中在 GUI 主线程，游戏高帧率或窗口消息繁忙时会
产生队列堆积。当前方案改为原生 C++ 渲染路径。

### SwapChain/Present 阻塞

早期 DirectComposition 与 DXGI SwapChain 路径在无上限帧游戏中可能被桌面合成器
长时间阻塞。当前默认路径改用 `UpdateLayeredWindow`，避免等待交换链 Present。

### CPU 缩放和逐像素处理

逐像素 CPU 缩放会随窗口尺寸增加而放大开销。当前优先使用 D3D11 采样，再进行
一次必要的缓冲区提交。

### 多 GPU 共享限制

Spout 共享纹理通常要求接收端与发送端使用同一 GPU。程序会显示检测到的 GPU，并将
自动模式优先指向节能/核显；手动切换到另一块 GPU 时需要重新启动相关程序。

### 无上限帧率调度压力

游戏无上限运行时可能挤压桌面合成、浏览器和 VTube Studio 的线程。将叠加层帧率
设为 VTS 或显示器的固定刷新率，通常比盲目提高目标帧率更稳定。

## 7. 构建

在安装 Visual Studio 2022 C++ 工具链的 Windows 环境中运行：

```powershell
.\native\build_native.ps1 -Configuration Release
```

输出为仓库根目录的 `VTSFloat_Meow.exe`。构建目录和编译产物不应提交到仓库。

## 8. 使用前提

1. 启动 VTube Studio。
2. 在 VTube Studio 中开启 Spout2，并设置透明背景。
3. 启动 `VTSFloat_Meow.exe` 或 `start_overlay.cmd`。
4. 如出现黑屏或无法接收，先确认发送端名称、GPU 选择和 VTube Studio 是否正在运行。

## 9. 目录说明

```text
VTSFloat_Meow.exe          可直接运行的程序
native/                    C++ 源码、CMake 配置和 Spout2 源码
md/                        面向读者的原理与性能说明
README.md                  快速介绍与使用说明
start_overlay.cmd          启动便捷脚本
stop_overlay.cmd           关闭便捷脚本
```

个人调试记录、绝对路径、运行日志和本地缓存不属于公开项目文档。
