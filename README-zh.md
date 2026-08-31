# CodexUsageBar

原生 `Win32/C++` 的 Codex 用量桌面挂件，面向 Windows。

[English README](README.md)
[![Linux.do 社区](https://img.shields.io/badge/Linux.do-%E7%A4%BE%E5%8C%BA-2ea44f?style=flat-square)](https://linux.do/)

> 本仓库是 [`luodaoyi/codex-useage-win`](https://github.com/luodaoyi/codex-useage-win) 的 Fork。当前 Fork 在上游限额面板基础上增加了 **Codex Desktop 本地 Token 统计、逐 Turn 的 API 等价成本估算、Primary Model 回退估算，以及 Standard / Simple / Taskbar 三种卡片式显示模式**。上游来源与提交历史保持可追溯。

## 上游项目看板

以下徽章和 Release 链接指向 upstream：

[![Build](https://github.com/luodaoyi/codex-useage-win/actions/workflows/build.yml/badge.svg)](https://github.com/luodaoyi/codex-useage-win/actions/workflows/build.yml)
[![Latest Release](https://img.shields.io/github/v/release/luodaoyi/codex-useage-win?display_name=tag)](https://github.com/luodaoyi/codex-useage-win/releases/latest)
[![Release Date](https://img.shields.io/github/release-date/luodaoyi/codex-useage-win)](https://github.com/luodaoyi/codex-useage-win/releases/latest)
[![Total Downloads](https://img.shields.io/github/downloads/luodaoyi/codex-useage-win/total)](https://github.com/luodaoyi/codex-useage-win/releases)
[![Stars](https://img.shields.io/github/stars/luodaoyi/codex-useage-win?style=flat)](https://github.com/luodaoyi/codex-useage-win/stargazers)

## 当前能力

CodexUsageBar 会读取当前 Codex 账号的限额信息，并读取本机 Codex Desktop 会话目录中的 **usage metadata**，用于同时观察额度、Token 和 Token API 等价成本。

### 远程额度

- `5H` 剩余额度：仅在后端实际提供 5 小时窗口时显示
- `周` 剩余额度：显示当前 weekly quota 的剩余百分比
- 周期重置时间、额度进度、重置卡等上游已有能力仍保留在 Standard 模式

### 本地 Token / Token API 等价成本

本地统计来自已解析的 Codex session usage metadata，不按聊天文本长度猜 Token。

- `Task`：当前 / 最新本地 Codex Task 的累计 Token + API 等价成本
- `Last`：当前 Task 最近一次可用的 turn usage
- `Today`：按 **Windows 本地自然日**统计的 Token + API 等价成本
- `周消费`：从远端 weekly quota 周期起点到现在的 API 等价成本，不是自然周
- `总消费`：当前可发现本地 Codex session 历史的累计 Token / API 等价成本

API 等价成本按模型及 Token 类别计算，包括可验证的 uncached input、cached input、cache-write input 和 output；它是“如果同样 Token 用量按公开 API 价格计费，大约值多少钱”的比较指标，**不是 ChatGPT / Codex 订阅账单，也不是 OpenAI Invoice**。

#### Cost Accuracy V2 如何提高准确度

- 价格目录离线内置并带版本 / 核验日期，目前覆盖 GPT-5.6 Sol / Terra / Luna、GPT-5.5、GPT-5.4、GPT-5.4 Mini、GPT-5.3-Codex、GPT-5.2 等当前支持模型；`gpt-5.6` 作为 Sol 的已知别名处理。
- 模型归因优先使用 Codex rollout 中真实的 canonical model metadata；不会从任意嵌套 `model` 字段或看起来相似的字符串猜模型。
- 成本按可重建的 **Turn / Request** 逐条计算：当 `last_token_usage` 结构合法并与本次累计增量一致时优先使用它；否则保守回退到非负 cumulative delta。
- 重复的 cumulative snapshot 会先去重，不会重复产生同一笔成本。
- 对官方规则明确适用的模型，单次请求 input 超过 `272,000` Token 时，会在该请求层面应用对应 long-context 倍率；不会把整个 session 汇总后误套倍率。
- cache-write 只在官方适用价格规则可确认时计价；无法确认的部分保留为未定价，而不是猜测。

### Primary Model / 主模型

右键菜单可以选择：

`API Equivalent - Primary model -> Auto | GPT-5.6 Sol | GPT-5.6 Terra | GPT-5.6 Luna | GPT-5.5`

- 默认是 `Auto`：只相信真实模型归因，不做用户猜测。
- 选择例如 `GPT-5.6 Terra` 后，只有**确实缺失可信模型归因**的 usage 才会按 Terra 做估算。
- Primary Model **不会覆盖**已经明确识别出来的其他 OpenAI 模型、第三方模型（例如 DeepSeek）或身份明确但当前无可靠价格的内部 / 未知模型。
- 设置保存在现有 `%APPDATA%\CodexUsageBar\settings.ini` 中；切换后成本会立即重新计算，不需要重装或重启。

显示语义：

- `≈$X.XX`：全部由可信模型归因和完整价格规则得到的 confirmed API 等价成本
- `~$X.XX`：包含 Primary Model fallback，属于 estimated API 等价成本
- `≥$X.XX`：至少可以确认这么多；仍存在无法可靠定价的明确模型 / 价格组件
- `≥$X.XX +~$Y.YY`：紧凑模式下同时存在 confirmed 下界、Primary Model estimated 部分以及仍未定价 usage；estimated 部分不会被伪装成数学下界

Standard 模式会显示当前选择的主模型，并在需要时把 confirmed / estimated / unpriced 语义拆开；Simple / Taskbar 保持紧凑，只改变金额标记和必要的 mixed-cost 文本。

## 示例

### Standard / 完整模式

保留账号、套餐、周额度、重置卡和刷新控制，并增加独立“本地用量”区域：`Task` / `Last` / `Today` / `周消费` / `总消费`。Cost Accuracy V2 下，该区域还会显示当前 `主模型`，并在存在 fallback 时区分 confirmed 与 estimated 金额。

![CodexUsageBar 标准模式](IMG/1.png)

### Simple / 简单模式

紧凑的可移动卡片，只保留最高价值信息：可选 `5H`、`周`、`周消费`、`总消费`。

当前布局示意：

<img width="603" height="278" alt="image" src="https://github.com/user-attachments/assets/e29c3920-2b5b-4a95-83b8-289a9988d104" />

当总消费同时包含 confirmed、Primary Model estimated 和仍未定价 usage 时，紧凑金额可能显示为类似：`≥$233.17 +~$46.63`。

### Taskbar / 任务栏模式

固定贴近当前显示器任务栏边缘，采用内容驱动宽度的独立圆角小卡片；没有 5H 数据时会整张隐藏 5H 卡片。

当前布局示意：

<img width="497" height="93" alt="image" src="https://github.com/user-attachments/assets/daf07d2b-0c13-4023-85f0-d2174e4eab1f" />

Taskbar 与 Simple 使用相同的 compact cost 语义；选择 Primary Model 后，如果确实存在 fallback usage，可显示 `~$...` 或 `≥$... +~$...`。

## 功能

- 原生 `Win32 + Direct2D + DirectWrite + WinHTTP`
- 无 `C#`、无 `WebView`、无 Electron
- 读取 `%USERPROFILE%\.codex\auth.json` 或 `%CODEX_HOME%\auth.json`
- 请求 `GET https://chatgpt.com/backend-api/wham/usage`
- 读取 `%USERPROFILE%\.codex\sessions` / `%CODEX_HOME%\sessions` 下的本地 usage metadata
- 三种显示模式：
  - **Standard / 完整模式**：账号 / 套餐 / 额度 / 重置卡 + Task / Last / Today / 周消费 / 总消费
  - **Simple / 简单模式**：可选 5H + 周 + 周消费 + 总消费，保持明显比 Standard 更小
  - **Taskbar / 任务栏模式**：同类高价值指标的紧凑横向卡片，自动贴近任务栏并按内容自适应宽度
- 本地会话统计按累计 snapshot 做去重，避免重复累计同一 session 的 cumulative token count
- 成本使用逐 Turn / Request 的内存 ledger：可信 `last_token_usage` 优先，否则回退 cumulative delta
- 模型归因只使用已识别的 Codex canonical rollout metadata，不使用任意嵌套 `model` 字段猜测
- 支持 Primary Model fallback，并把 confirmed / estimated / unpriced 金额分开表达
- 对当前官方规则适用的模型支持 >272K long-context 逐请求定价
- 支持 Light / Dark theme 和 Windows DPI 缩放
- 桌面浮层挂件，可拖动、可缩放（Taskbar 模式固定贴边）
- 支持开机自启、Always on top、Lock position
- 支持中英文界面切换

## 隐私与安全

- `auth.json`、`access_token`、`refresh_token`、`id_token` 不会进入 README、测试 fixture 或普通日志
- 本地 session 解析器只提取 usage / model 等统计所需结构；不展示、记录或上传 prompt、assistant 内容、代码 / tool 内容
- Cost ledger 只在内存中重建统计增量，不持久化原始 JSONL、Prompt 或响应内容
- 自动测试使用 synthetic / redacted fixture，不复制真实 Codex session 或真实账号凭据
- API 等价成本只在本机统计结果上计算，不需要额外 API Key，也不会为了价格计算新增网络请求

## 刷新策略

- 远程额度接口：默认每 `60` 秒刷新一次
- 本地 Token / API 等价统计：后台约每 `5` 分钟重新扫描一次；程序启动时会立即进行首次扫描
- 本地倒计时 / UI：每 `1` 秒重绘一次
- 右键 `立即刷新`：立即刷新远程额度
- 切换 Primary Model：直接基于当前内存统计重新计算成本，不要求重新扫描或重启程序
- OAuth Token：当前实现支持接近过期时自动 refresh，并支持 401 / 403 后的恢复刷新；右键菜单也提供手动 `Refresh Token`

## 使用方式

- 拖动挂件主体：移动位置
- 拖右边、下边、右下角：调整大小
- 任务栏模式固定贴边显示，不支持拖动或缩放
- 右键菜单包含：
  - `立即刷新`
  - `Refresh Token`
  - `开机自启`
  - `始终置顶`
  - `固定位置`
  - `显示模式`
  - `API Equivalent - Primary model`
  - `语言`
  - `重置组件位置`
  - `退出`

位置、尺寸和 Primary Model 设置保存到：

- `%APPDATA%\CodexUsageBar\settings.ini`

开机自启使用当前用户注册表：

- `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`

## 本地构建

### 直接构建

```cmd
build.cmd
```

输出文件：

- `CodexUsageBar.exe`

### 使用 CMake

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

项目同时包含针对本地 Token accounting / pricing / presentation 的 CTest 测试。

## GitHub Actions

仓库 workflow 支持：

- `x64` / `ARM64` 构建
- `pull_request`
- push 到 `main` / `master`
- `workflow_dispatch`
- `v*` Tag 触发公开 GitHub Release

> 注意：`v*` Tag 会触发公开 Release。当前 Fork 尚未发布公开 Release；在创建版本 Tag / 对外分发二进制前，应先确认上游许可 / 授权和当前发布决策。

## 已知限制

- `chatgpt.com/backend-api/wham/usage` 及其字段不是本项目控制的稳定公开 contract；后端字段变化时可能需要更新解析
- 本地统计依赖 Codex session JSONL / rollout schema；未来 Codex schema 变化时可能需要适配
- API 等价成本依赖当前离线维护的模型价格覆盖；历史、第三方或身份明确但无可靠公开价格的模型会保留为未定价，而不是被 Primary Model 强制覆盖
- Token API Equivalent 目前主要计算可验证的 Token 价格；Web Search、Image Generation、Fast Mode、Regional Processing 或其他独立按次 / 附加计费项目不会在缺少明确本地 telemetry 和适用价格证据时被猜进 headline 成本
- >272K long-context 规则只对当前官方文档明确适用的模型 / 请求应用；未来官方定价变化需要通过软件更新维护离线价格目录
- `总消费` 仅覆盖当前可读取、可发现的本地 session 历史，不等于 OpenAI 账户侧完整历史账单
- 当前是桌面浮层挂件，不是 Windows 7 时代的官方 Gadget 平台
- upstream 当前未声明明确 LICENSE；对外再分发本 Fork 二进制前请先核验授权 / 许可证要求

## Star History

<a href="https://www.star-history.com/?repos=luodaoyi%2Fcodex-useage-win&type=timeline&logscale=&legend=top-left">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=luodaoyi/codex-useage-win&type=timeline&theme=dark&logscale&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=luodaoyi/codex-useage-win&type=timeline&logscale&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/chart?repos=luodaoyi/codex-useage-win&type=timeline&logscale&legend=top-left" />
 </picture>
</a>
