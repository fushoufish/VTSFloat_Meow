# GitHub 更新代理研究与实测

2026-11-11 对三个候选节点进行过实测。实现采用 GitHub 原生与节点 1，以下保留当时的测试结果。

## 1. 测试方法与基准

对本项目最新正式 Release 发起无登录、无私人令牌的 HTTP GET，读取并解析完整元信息；对 EXE 下载完整响应并在内存计算 SHA-256，不执行下载内容；用项目现有 README 验证 Raw 文件传输。

基准来自 GitHub 原生 Release API 和完整 EXE 下载：

```text
版本：v1.2.1
文件：VTSFloat_Meow.exe
字节数：2082816
SHA-256：fd7f68930ce8e2bdbac0c9d53d0bf7e3f1fde4630feaa9bc9181c158b007b7f6

版本 API：
https://api.github.com/repos/fushoufish/VTSFloat_Meow/releases/latest

EXE：
https://github.com/fushoufish/VTSFloat_Meow/releases/download/v1.2.1/VTSFloat_Meow.exe

Raw 测试文件：
https://raw.githubusercontent.com/fushoufish/VTSFloat_Meow/main/README.md
```

原生 API、Raw 和 EXE 均返回 200；API 附件的 `size`、`digest` 与实际下载完全一致。代理成功与否按响应和内容判断，不采用截图里的“可用”标记作为测试结论。本次结果来自当前网络，不构成中国大陆各运营商的测速结果或持续可用承诺。

## 2. 三站结果

| 顺序 | 代理 | latest API | Raw | 本项目 EXE | 本轮结论 |
| --- | --- | --- | --- | --- | --- |
| 1 | `https://gh-proxy.com/` | 200，可解析 v1.2.1 | 200 | 200，字节数与哈希一致 | 版本检查与下载均通过 |
| 2 | `https://ghproxy.net/` | 403 `Invalid input.` | 200，与原生文件相同 | 429，GitHub 上游限流页 | API 不适用；版本检查需 Raw 清单，下载当前受限 |
| 3 | `https://ghproxy.homeboyc.cn/` | 403 `blocked` | 403 `blocked` | 403 `blocked` | 当前请求被拒绝，不算可用 |

### 节点 1

服务文档声明支持 GitHub API、Raw、固定版本 Release 附件，格式为“代理域名 + 原始完整 URL”。本项目实测通过：

```text
https://gh-proxy.com/https://api.github.com/repos/fushoufish/VTSFloat_Meow/releases/latest
https://gh-proxy.com/https://github.com/fushoufish/VTSFloat_Meow/releases/download/v1.2.1/VTSFloat_Meow.exe
https://gh-proxy.com/https://raw.githubusercontent.com/fushoufish/VTSFloat_Meow/main/README.md
```

无需先打开站点主页或手动转换，也不需要客户端自动操作网页。

来源：[服务使用文档](https://gh-proxy.com/docs/github-accelerator)。

### 节点 2

站点介绍文件、Release 和源码归档加速，未声明支持 GitHub API；公开上游实现的 URL 规则也没有 API 类型。实测 API 请求被直接拒绝，而 Raw 文件能正常读取：

```text
版本 API（不适用）：
https://ghproxy.net/https://api.github.com/repos/fushoufish/VTSFloat_Meow/releases/latest

Raw 文件（成功）：
https://ghproxy.net/https://raw.githubusercontent.com/fushoufish/VTSFloat_Meow/main/README.md

EXE（本轮返回 429）：
https://ghproxy.net/https://github.com/fushoufish/VTSFloat_Meow/releases/download/v1.2.1/VTSFloat_Meow.exe
```

接入方法：正式发布后自动生成一个小型 JSON 版本清单，存放在 GitHub 专用 `updates` 分支，通过以下 Raw 地址经本站代理读取，解析版本、固定下载地址、大小和哈希：

```text
https://ghproxy.net/https://raw.githubusercontent.com/fushoufish/VTSFloat_Meow/updates/stable.json
```

该地址为拟新增方案，版本清单尚未在本轮创建或发布；成功读取 README 仅验证了 Raw 传输能力，不能写成上述清单已经联调成功。

429 是服务端/上游当前的限制，换一种 URL 格式不能保证消除；客户端应记录失败并尝试下一个节点，不立即反复请求该站。

来源：[站点说明](https://ghproxy.net/)、[上游代理 URL 规则](https://raw.githubusercontent.com/hunshcn/gh-proxy/master/index.js)。在线服务的实际配置可能与公开上游不同，实测结果优先。

### 节点 3

站点与其公开源码声明支持 API 和 Release，使用同样的 URL 前缀格式：

```text
https://ghproxy.homeboyc.cn/https://api.github.com/repos/fushoufish/VTSFloat_Meow/releases/latest
https://ghproxy.homeboyc.cn/https://github.com/fushoufish/VTSFloat_Meow/releases/download/v1.2.1/VTSFloat_Meow.exe
https://ghproxy.homeboyc.cn/https://raw.githubusercontent.com/fushoufish/VTSFloat_Meow/main/README.md
```

但本项目三类请求均返回 403 `blocked`。公开源码在白名单不匹配时使用相同响应，因此推测部署启用了白名单；实际部署配置未公开，不能仅据此断定全部原因。无需尝试绕过该限制。

可按用户要求保留为第三次重连，但遇到当前拒绝便将本次节点判为失败。它的主页可打开，不等于本项目的更新请求获准通过。

来源：[站点说明](https://ghproxy.homeboyc.cn/)、[公开实现的白名单检查](https://raw.githubusercontent.com/asjdf/ghproxy/main/src/index.ts)、[公开配置示例](https://raw.githubusercontent.com/asjdf/ghproxy/main/src/config.example.ts)。

## 3. 实现采用的路线

GitHub 原生 → gh-proxy.com。检查与下载分别按该顺序尝试，各路线最多一次。两路检查都失败时显示“检查失败”。

节点 1 能直接转发本项目的 GitHub Release API。客户端下载同一标签的 EXE，并使用 Release 的大小和 SHA-256 校验，随后校验 Windows 文件版本和架构。

同一服务的 gh-proxy.org 也实测返回 v1.2.1 和正确的 EXE 附件信息。本地实现使用 gh-proxy.com。

CNB 的公开 EXE 下载地址本次返回 200。若后续要提供另一条大陆分发路线，可以把版本 JSON 和 EXE 一起同步为公开附件。客户端可通过普通文件请求读取清单。现有 CNB Release API 的身份验证属于管理接口。

- [GH-Proxy 服务文档](https://gh-proxy.com/docs/github-accelerator)
- [CNB 公开 EXE](https://cnb.cool/fushoufish/VTSFloat_Meow/-/releases/latest/download/VTSFloat_Meow.exe)
- [CNB Open API 认证说明](https://docs.cnb.cool/zh/develops/openapi.html)
