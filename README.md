<p align="center">
  <img src="docs/assets/VTSFloat_Meow.webp" alt="VTSFloat_Meow Logo" width="240">
</p>
<h1 align="center">VTSFloat_Meow</h1>
<p align="center">
  <a href="https://github.com/fushoufish/VTSFloat_Meow/releases/latest"><img src="https://img.shields.io/github/v/release/fushoufish/VTSFloat_Meow?style=flat-square&amp;label=Release&amp;color=0078D4" alt="Release"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-0078D4?style=flat-square&amp;logo=cplusplus&amp;logoColor=white" alt="C++">
  <img src="https://img.shields.io/badge/Windows%2010%2F11%20x64-0078D4?style=flat-square" alt="Windows 10/11 x64">
  <a href="https://github.com/leadedge/Spout2"><img src="https://img.shields.io/badge/Spout2-0078D4?style=flat-square" alt="Spout2"></a>
  <a href="./LICENSE"><img src="https://img.shields.io/badge/GPL--3.0-0078D4?style=flat-square" alt="GPL-3.0"></a>
</p>

<p align="center">将 VTube Studio 的透明模型不借助其他工具直接悬浮到桌面、游戏或其他应用上方。</p>
<p align="center">高性能，轻量，开源</p>
<p align="center">支援简体、繁體中文、English、日本語、한국어、Русский与自定义语言。</p>

<p align="center">
  <a href="https://fushoufish.github.io/VTSFloat_Meow/">前往 VTSFloat_Meow 的介绍页查看教程与说明细节</a><br>
  <a href="https://fushoufish.github.io/VTSFloat_Meow/">Visit the VTSFloat_Meow Tutorials &amp; Documentation</a>
</p>


---


## 1. 前话

许多用户（~~包括我~~）希望把 VTube Studio 模型放到游戏、桌面或其他应用上方，
然后美美地（~~傻傻地~~）欣赏自己的模型动态。

邪修方案通常需要借助 OBS、Bandicam 等工具完成采集、抠图和窗口叠加，
也会增加额外的编码与合成开销。
VTSFloat_Meow 直接接收 VTube Studio 的 Spout2 共享纹理，
并通过原生透明窗口显示模型，使其不依赖推流软件。

### 如果电脑同时配有核显和独显，建议将 VTube Studio 与 VTSFloat_Meow 统一切换到核显/节能 GPU

#### 为什么 (,,Ծ‸Ծ,,)？

> **当独显（高性能显卡）运行高负载程序或游戏时，可能导致 VTSFloat_Meow 输出的透明模型异常卡顿。** 极限负载会大量占用独显的渲染队列、显存带宽和调度时间，使模型渲染、共享纹理接收以及 Windows 透明窗口合成得不到及时执行，从而出现掉帧、延迟甚至短暂卡死。

#### 为什么录屏或其他软件仍可以在极限负载场景正常运行？

> 录屏主要在 GPU 内部完成，而 VTSFloat_Meow 还依赖 Windows 桌面透明窗口合成。
>
> VTSFloat_Meow 的 Spout2 接收和模型缩放已经通过 D3D11 在 GPU 内完成，但最后仍需由 Windows 桌面合成器显示透明窗口。独显满载时，桌面合成得不到足够调度，因此比主要在 GPU 内部工作的录屏软件更容易卡顿。纯 GPU 的 DirectComposition 同样依赖桌面合成，甚至可能出现 Present 长时间阻塞；除非注入游戏内部绘制，但会带来反作弊和兼容风险。因此，“GPU 处理 + Windows 透明窗口上屏”是目前稳定的非注入方案之一。

#### 使用过程中非常卡，或电脑只有一块 GPU 怎么办？

> 请降低高负载程序或游戏的图形设置，尤其要限制游戏帧率。无上限帧率会持续占用 GPU 调度资源，可以先将上限调整为 60 FPS 或更低再进行尝试。

## 2. 主要功能

- 无需 OBS、Bandicam 等额外工具，即可将 VTube Studio 的透明模型显示在桌面、其他应用及窗口化或无边框全屏游戏上方。
- 支持“悬浮窗口”和“桌面模式”切换；桌面模式按普通窗口显示，也可在其他程序全屏时自动暂停并在退出全屏后恢复。
- 模型窗口可自由缩放、锁定比例或自由拉伸，并支持多显示器切换与位置记忆。
- 锁定后自动启用鼠标穿透，不影响操作模型下方的桌面、游戏或应用；可通过全局快捷键快速锁定或解锁。
- 可调整模型不透明度、鼠标悬停时的透明程度和触发范围，并支持手动框选多个独立模型区域。
- 支持选择多个 VTube Studio 表情作为悬停互动效果，并设置播放时长、恢复延时和平滑透明过渡。
- 支持界面缩放、帧率模式、GPU 选择、系统托盘和窗口隐藏；内置六种语言，也可载入自定义语言。

### VNet / 多人联动与框选范围

理论上支持 VTube Studio 的 VNet 多人联动。画布中除空背景外，任何不透明区域都会被判定为“模型区域”；天气、粒子和其他覆盖画面的特效也会进入自动触发范围，使“触碰隐藏”和悬停表情等功能无法准确识别角色。遇到这种情况时，请启用手动框选范围，为一个或多个角色限定独立的鼠标触发区域。

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
UpdateLayeredWindow 提交为 Win32 透明分层窗口
```

Spout2 接收端已经集成到程序中，用户不需要单独安装或启动 Spout2。

开发进度见 [开发进度与任务流](./md/开发进度与任务流.md)。渲染管线、透明合成、GPU 同步和性能演进见 [项目说明_实现原理与性能演进](./md/项目说明_实现原理与性能演进.md)。

### 本地处理与隐私

项目网站的隐私政策见：[隐私政策 / Privacy Policy](https://fushoufish.github.io/VTSFloat_Meow/privacy.html)。

- 程序在桌面独立合成模型。开启背景混合或阴影的背景取色时，程序会在本机采样模型周围的屏幕底色。
- 模型悬浮可离线使用。VTube Studio Plugins API 通过 `localhost` 与本机 VTube Studio 通信。
- 检查和下载更新会请求 GitHub 公开 Release，原生连接失败时尝试 `gh-proxy.com`。
- 程序不会访问 VTube Studio 使用的摄像头或其他连接设备。
- 接收到的模型画面只在本机 GPU 与 Windows 桌面合成链路中处理，不会上传或录制保存。

## 4. 快速开始

1. 从 [Releases](https://github.com/fushoufish/VTSFloat_Meow/releases/latest) 下载最新版并解压。
2. 启动 VTube Studio，打开“设置 → 相机”。
3. 启用 **Spout2**，背景选择 `ColorPicker`，然后开启**透明推流**。
4. 运行 `VTSFloat_Meow.exe`。程序会从注册表查找 Steam 与 VTube Studio 路径；未找到时可手动选择。
5. 首次使用 API 功能时，在 VTube Studio 中允许 `VTSFloat_Meow` 插件连接。
6. 调整窗口后点击“完成”锁定并隐藏工具栏。首次点击“完成”会显示操作指引。按 `Ctrl+Shift+L` 可重新打开工具栏，点“快捷键”可直接设置组合键。

### 检查更新

鼠标停在工具栏的 GitHub 图标上可查看当前版本，并点击“检查更新”。程序只在用户点击后检查版本。“暂不更新”和 × 都会关闭本次提示。

选择“更新并重启”后，程序会校验下载、保存当前设置，并在原路径替换 EXE。新版确认启动并显示成功提示后清理旧版备份，启动失败时恢复旧版。

### VTube Studio API

仅显示透明模型不需要 API；若需要悬停表情和 VTS 实时帧率，请完成以下设置：

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

首次启动时，程序会创建专用缓存目录 `%LOCALAPPDATA%\VTSFloat_Meow`。鼠标停在工具栏的 GitHub 图标上，从“调试 → 打开缓存文件夹”打开。

程序设置保存在：

```text
%LOCALAPPDATA%\VTSFloat_Meow\vts_overlay_layered.ini
```

其中包括窗口位置、尺寸、比例、帧率、GPU、边框、悬停范围、表情和快捷键。可通过 GitHub 菜单中的“调试 → 清除缓存并重置脚本”恢复默认设置。专用目录中没有设置文件时，程序会迁移旧版 Local 设置，并转换已知旧字段。

运行日志保存在缓存目录中的 `vts_overlay_layered.log`，更新任务保存在 `VTSFloat_Meow-updates` 子目录。

首次启动且尚未保存语言偏好时，程序会按照 Windows 显示语言自动选择简体中文、繁体中文、
English、日本語、한국어或Русский；后续启动会记住用户在语言菜单中的选择。

程序会在缓存目录中生成自定义语言模板 `VTSFloat_Meow.custom-language.ini`。
该模板以英文为对照，保留左侧
`T_` 键并修改等号与 ` #英文备注` 之间的文字即可制作新的语言；行尾备注不会显示，缺失或留空的内容自动使用英文。可在
“Language → Edit custom language file…”中打开，保存后重新选择 `Custom (INI)` 即可载入。

## 7. 使用前提

- Windows 10/11 x64 与支持 Direct3D 11 的硬件 GPU。
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
VTSFloat_Meow/
├─ .gitignore                             Git 忽略规则
├─ README.md                              项目介绍、使用方法与构建说明
├─ LICENSE                                GNU GPL v3.0 项目许可证
├─ THIRD_PARTY_NOTICES.md                 Spout2 与 Viewer.js 第三方许可证
├─ VTSFloat_Meow.exe                      Release 构建生成的主程序
├─ VTSFloat_Meow.custom-language.ini      仓库附带的自定义语言参考模板
├─ start_overlay.cmd                      启动主程序
├─ stop_overlay.cmd                       正常关闭主程序
├─ st_vts_api.png                         VTube Studio API 设置示意图
│
├─ md/
│  └─ 项目说明_实现原理与性能演进.md       渲染链路与性能演进记录
│
└─ native/                                Windows 原生程序源码
   ├─ CMakeLists.txt                      CMake 目标、依赖与测试配置
   ├─ build_native.ps1                    MSVC x64 Release 构建脚本
   ├─ check_localization.ps1              本地化完整性检查
   ├─ overlay_layered.cpp                 主程序、窗口、渲染与交互逻辑
   ├─ desktop_mode_policy.h               桌面模式全屏判定规则
   ├─ desktop_mode_tests.cpp              桌面模式判定测试
   ├─ localization.h                      本地化接口与语言枚举
   ├─ localization.cpp                    内置翻译与自定义语言加载
   ├─ localization_layout_audit.cpp       多语言界面宽度审计
   ├─ overlay_dx11.cpp                    D3D11 覆盖层实验程序
   ├─ spout_bridge.cpp                    Spout2 接收桥接工具
   ├─ spout_test_sender.cpp               Spout2 测试发送端
   ├─ overlay_resources.rc                Windows 图标与资源脚本
   ├─ resource.h                          Windows 资源标识
   ├─ github.svg                          GitHub 图标源文件
   ├─ github_mark.h                       GitHub 矢量图标绘制
   ├─ assets/
   │  └─ VTSFloat_Meow.ico                主程序图标
   └─ third_party/Spout2/                 内置 Spout2 第三方源码与许可证
```

`native/build/`、`native/bin/` 和 `output/` 属于本地构建、测试或采集生成目录，不计入源文件结构。

## 开源协议

VTSFloat_Meow 采用 [GNU General Public License v3.0](./LICENSE) 发布。您可以使用、研究、修改和重新分发本程序；公开分发修改版或衍生版本时，需要遵守 GPLv3 并提供对应源代码。


本项目包含独立授权的第三方组件：

- **Spout2** — BSD 2-Clause License. Copyright and license notices must be retained.
- **Viewer.js**：仅用于 `docs/` 文档网站的图片预览，采用 MIT License，不属于 Windows 桌面程序的运行依赖。

完整内容见 [第三方许可证声明](./THIRD_PARTY_NOTICES.md)。第三方组件继续适用各自的许可证，不因本项目采用 GPLv3 而改变。
