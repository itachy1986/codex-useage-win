# CodexUsageBar

原生 `Win32/C++` 的 Codex 用量桌面挂件，面向 Windows。

[English README](README.md)
[![Linux.do 社区](https://img.shields.io/badge/Linux.do-%E7%A4%BE%E5%8C%BA-2ea44f?style=flat-square)](https://linux.do/)

> 本仓库是 [`luodaoyi/codex-useage-win`](https://github.com/luodaoyi/codex-useage-win) 的 Fork。当前 Fork 在上游限额面板基础上增加了 **Codex Desktop 本地 Token 统计、API 等价成本估算，以及 Standard / Simple / Taskbar 三种卡片式显示模式**。上游来源与提交历史保持可追溯。

## 上游项目看板

以下徽章和 Release 链接指向 upstream：

[![Build](https://github.com/luodaoyi/codex-useage-win/actions/workflows/build.yml/badge.svg)](https://github.com/luodaoyi/codex-useage-win/actions/workflows/build.yml)
[![Latest Release](https://img.shields.io/github/v/release/luodaoyi/codex-useage-win?display_name=tag)](https://github.com/luodaoyi/codex-useage-win/releases/latest)
[![Release Date](https://img.shields.io/github/release-date/luodaoyi/codex-useage-win)](https://github.com/luodaoyi/codex-useage-win/releases/latest)
[![Total Downloads](https://img.shields.io/github/downloads/luodaoyi/codex-useage-win/total)](https://github.com/luodaoyi/codex-useage-win/releases)
[![Stars](https://img.shields.io/github/stars/luodaoyi/codex-useage-win?style=flat)](https://github.com/luodaoyi/codex-useage-win/stargazers)

## 当前能力

CodexUsageBar 会读取当前 Codex 账号的限额信息，并读取本机 Codex Desktop 会话目录中的 **usage metadata**，用于同时观察额度、Token 和 API 等价成本。

### 远程额度

- `5H` 剩余额度：仅在后端实际提供 5 小时窗口时显示
- `周` 剩余额度：显示当前 weekly quota 的剩余百分比
- 周期重置时间、额度进度、重置卡等上游已有能力仍保留在 Standard 模式

### 本地 Token / API 等价成本

本地统计来自已解析的 Codex session usage metadata，不按聊天文本长度猜 Token。

- `Task`：当前 / 最新本地 Codex Task 的累计 Token + API 等价成本
- `Last`：当前 Task 最近一次可用的 turn usage
- `Today`：按 **Windows 本地自然日**统计的 Token + API 等价成本
- `周消费`：从远端 weekly quota 周期起点到现在的 API 等价成本，不是自然周
- `总消费`：当前可发现本地 Codex session 历史的累计 Token / API 等价成本

API 等价成本按模型及 Token 类别计算，包括可验证的 uncached input、cached input、cache-write input 和 output；它是“如果同样用量按 API 公开价格计费，大约值多少钱”的比较指标，**不是 ChatGPT / Codex 订阅账单**。

显示语义：

- `≈$X.XX`：该统计范围内的用量可以完整按当前已支持价格计算
- `≥$X.XX`：至少可以确认这么多；统计范围中还存在无法可靠定价的模型 / 用量，未知部分不会被偷偷按 `$0` 计算

## 示例

### Standard / 完整模式

保留账号、套餐、周额度、重置卡和刷新控制，并增加独立“本地用量”区域：`Task` / `Last` / `Today` / `周消费` / `总消费`。

![CodexUsageBar 标准模式](IMG/1.png)

### Simple / 简单模式

紧凑的可移动卡片，只保留最高价值信息：可选 `5H`、`周`、`周消费`、`总消费`。

当前布局示意：

<img width="603" height="278" alt="image" src="https://github.com/user-attachments/assets/e29c3920-2b5b-4a95-83b8-289a9988d104" />

### Taskbar / 任务栏模式

固定贴近当前显示器任务栏边缘，采用内容驱动宽度的独立圆角小卡片；没有 5H 数据时会整张隐藏 5H 卡片。

当前布局示意：

<img width="497" height="93" alt="image" src="https://github.com/user-attachments/assets/daf07d2b-0c13-4023-85f0-d2174e4eab1f" />


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
- 模型归因只使用已识别的 Codex canonical rollout metadata，不使用任意嵌套 `model` 字段猜测
- 支持 Light / Dark theme 和 Windows DPI 缩放
- 桌面浮层挂件，可拖动、可缩放（Taskbar 模式固定贴边）
- 支持开机自启、Always on top、Lock position
- 支持中英文界面切换

## 隐私与安全

- `auth.json`、`access_token`、`refresh_token`、`id_token` 不会进入 README、测试 fixture 或普通日志
- 本地 session 解析器只提取 usage / model 等统计所需结构；不展示、记录或上传 prompt、assistant 内容、代码 / tool 内容
- 自动测试使用 synthetic / redacted fixture，不复制真实 Codex session 或真实账号凭据
- API 等价成本只在本机统计结果上计算，不需要额外 API Key

## 刷新策略

- 远程额度接口：默认每 `60` 秒刷新一次
- 本地 Token / API 等价统计：后台约每 `5` 分钟重新扫描一次；程序启动时会立即进行首次扫描
- 本地倒计时 / UI：每 `1` 秒重绘一次
- 右键 `立即刷新`：立即刷新远程额度
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
  - `语言`
  - `重置组件位置`
  - `退出`

位置和尺寸保存到：

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
- API 等价成本依赖当前已维护的模型价格覆盖；历史或第三方 / 内部模型可能只能显示 `≥$...` 下界
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
