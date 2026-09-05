<p align="center">
  <img src="docs/assets/VTSFloat_Meow.png" alt="VTSFloat_Meow Logo" width="240">
</p>

<h1 align="center">VTSFloat_Meow</h1>

<p align="center">将 VTube Studio 的透明模型直接悬浮到桌面、游戏或其他应用上方。</p>

<p align="center"><strong>当前版本：v1.1（正式版）</strong></p>

## 1. 前话

许多用户（~~包括我~~）希望把 VTube Studio 模型放到游戏、桌面或其他应用上方，
然后美美地（~~傻傻地~~）欣赏自己的模型动态。

邪修方案通常需要借助 OBS、Bandicam 等工具完成采集、抠图和窗口叠加，
也会增加额外的编码与合成开销。
VTSFloat_Meow 直接接收 VTube Studio 的 Spout2 共享纹理，
并通过原生透明窗口显示模型，使其不依赖推流软件。

### 如您的电脑除了独显之外，还建议启用并切换核显/节能GPU进行使用

#### 为什么 (,,Ծ‸Ծ,,)？

> **当 独显(高性能显卡) 运行在 极限负载的程序/游戏中时，将会导致 VTSFloat_Meow 输出的透明模型异常卡顿**，极限负载状态会最大占用独显的渲染队列、显存带宽和调度时间，导致模型渲染、共享纹理接收以及 Windows 透明窗口合成都可能得不到及时执行，从而出现掉帧、延迟甚至短暂卡死。

#### 为什么录屏/其他软件仍可以在极限负载场景依然正常运行？

> 录屏主要在 GPU 内部完成，而 VTSFloat_Meow 还依赖 Windows 桌面透明窗口合成。
>
> VTSFloat_Meow 的 Spout2 接收和模型缩放已经通过 D3D11 在 GPU 内完成，但最后仍需由 Windows 桌面合成器显示透明窗口。独显满载时，桌面合成得不到足够调度，因此比主要在 GPU 内部工作的录屏软件更容易卡顿。纯 GPU 的 DirectComposition 同样依赖桌面合成，甚至可能出现 Present 长时间阻塞；除非注入游戏内部绘制，但会带来反作弊和兼容风险。因此，“GPU 处理 + Windows 透明窗口上屏”是目前稳定的非注入方案之一。

#### 我使用过程中 VTSFloat_Meow 非常卡 / 我只有一个GPU显卡怎么办？

> 请调整高负载程序/游戏的性能开销，特别针对显卡高负载的设置请降低其配置进行运行，**特别留意类似关于 "帧率上限" 相关的设置，这将极大占用显卡的性能开销，将其调整至60帧或者其他更低的帧数** 进行尝试。

## 2. 一些功能

- 将 VTube Studio 透明模型置顶显示在桌面、全屏游戏、无边框游戏或其他应用上方。
- 任意调整窗口大小，支持比例锁定、自由拉伸、多显示器兼容与切换及位置记忆。
- 锁定后支持鼠标穿透，仍可操作模型下方可交互的游戏或应用。
- 支持模型整体不透明度、鼠标悬停透明化、平滑过渡及悬停扩展范围。
- 支持手动框选多个独立悬停区域，并提供撤销、恢复和实时预览。
- 通过 VTube Studio Plugins API 获取并多选表情，实现鼠标移入时播放表情。
- 预览、重复悬停或正常退出程序时，恢复用户原先启用的完整表情状态。
- 提供固定帧率、跟随 VTS 配置和跟随 VTS 实时帧率等策略。
- 自动识别 Spout 发送端 GPU，支持多 GPU 选择及 VTube Studio 启动辅助。
- 支持系统托盘、全局快捷键、窗口隐藏/恢复
- 一些脚本自定义与调试设置。

## 3. 工作原理

```text
VTube Studio 渲染模型
        ↓
Spout2 在显存中共享透明纹理
        ↓
VTSFloat_Meow 使用 D3D11 接收并缩放
        ↓
生成预乘 Alpha BGRA 画面
        ↓
UpdateLayeredWindow 显示为 Win32 透明置顶窗口
```

Spout2 接收端已经集成到程序中，用户不需要单独安装或启动 Spout2。

完整的数据流、透明合成、GPU 同步、性能统计和早期瓶颈说明见：[项目说明：实现原理与性能演进](./md/项目说明_实现原理与性能演进.md)。

## 4. 快速开始

1. 启动 VTube Studio。
2. 打开 VTube Studio 的“设置 → 相机”。
3. 启用 **Spout2**，背景选择 `ColorPicker`，然后开启**透明推流**。
4. 运行 `VTSFloat_Meow.exe`。
5. 首次使用 API 功能时，在 VTube Studio 中允许 `VTSFloat_Meow` 插件连接。
6. 调整窗口后点击“完成”锁定；默认锁定/解锁快捷键为 `Ctrl+Shift+L`。

### VTube Studio API

若需要悬停表情和 VTS 实时帧率：

1. 在 VTube Studio 设置中找到 **VTube Studio Plugins / Plugin API**。
2. 开启 **Start API / Allow Plugin API access**，通常保持默认端口 `8001`。
3. 启动 VTSFloat_Meow，并在 VTS 弹出的授权提示中选择允许。
4. 若未自动连接，可在程序提示中点击“扫描端口”。

## 5. GPU 与性能建议

- 多 GPU 电脑推荐统一使用核显/节能 GPU，让游戏独占高性能显卡；只有一块 GPU 的电脑可以直接使用。
- VTube Studio、Spout2 发送端和 VTSFloat_Meow 应使用同一块 GPU，否则共享纹理可能无法打开。
- 建议让 VTS 画布与悬浮窗口的分辨率接近。尺寸越大，缩放、显存读写和透明窗口上屏成本越高。
- 优先使用“跟随 VTS 配置”。把覆盖层帧率设得高于 VTS 输出不会产生更多模型帧，只会重复处理同一画面。
- 高负载游戏不建议使用无上限帧率；它可能挤占 VTS、桌面合成器和覆盖层的 GPU 调度时间。

## 6. 设置与重置

程序设置保存在：

```text
%LOCALAPPDATA%\vts_overlay_layered.ini
```

其中包括窗口位置、尺寸、比例、帧率、GPU、边框、悬停范围、表情和快捷键。可通过“调试 → 清除缓存并重置脚本”恢复默认状态。

多语言版本会同时生成 `VTSFloat_Meow.custom-language.ini`。该模板以英文为对照，保留左侧
`T_` 键并修改等号右侧文字即可制作新的语言；缺失或留空的内容自动使用英文。可在
“Language → Edit custom language file…”中打开，保存后重新选择 `Custom (INI)` 即可载入。

## 7. 使用前提

- Windows 10/11 与支持 Direct3D 11 的硬件 GPU。
- 已安装并正常运行 VTube Studio。
- VTube Studio 已开启 Spout2 和透明推流。
- 使用表情功能时，需要授权 VTube Studio Plugins API。

## 构建

安装 Visual Studio 2022 C++ 工具链，在项目根目录运行：

```powershell
.\native\build_native.ps1 -Configuration Release
```

生成文件为根目录的 `VTSFloat_Meow.exe`。

## 项目结构

```text
VTSFloat_Meow.exe           可直接运行的程序
VTSFloat_Meow.custom-language.ini  自定义语言模板（多语言版本）
native/                     C++ 源码、CMake 配置和构建脚本
native/third_party/Spout2   内置 Spout2 源码
md/                         实现原理与性能演进文档
start_overlay.cmd           启动程序
stop_overlay.cmd            正常关闭程序
```
