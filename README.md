# CodexUsageBar

A native `Win32/C++` Codex usage widget for Windows.

[中文说明](README-zh.md)
[![Linux.do Community](https://img.shields.io/badge/Linux.do-Community-2ea44f?style=flat-square)](https://linux.do/)

> This repository is a fork of [`luodaoyi/codex-useage-win`](https://github.com/luodaoyi/codex-useage-win). This fork keeps the upstream quota widget and adds **local Codex Desktop token accounting, API-equivalent cost estimates, and card-based Standard / Simple / Taskbar modes**. Upstream attribution and history are preserved.

## Upstream Project Status

The badges and Release links below refer to upstream:

[![Build](https://github.com/luodaoyi/codex-useage-win/actions/workflows/build.yml/badge.svg)](https://github.com/luodaoyi/codex-useage-win/actions/workflows/build.yml)
[![Latest Release](https://img.shields.io/github/v/release/luodaoyi/codex-useage-win?display_name=tag)](https://github.com/luodaoyi/codex-useage-win/releases/latest)
[![Release Date](https://img.shields.io/github/release-date/luodaoyi/codex-useage-win)](https://github.com/luodaoyi/codex-useage-win/releases/latest)
[![Total Downloads](https://img.shields.io/github/downloads/luodaoyi/codex-useage-win/total)](https://github.com/luodaoyi/codex-useage-win/releases)
[![Stars](https://img.shields.io/github/stars/luodaoyi/codex-useage-win?style=flat)](https://github.com/luodaoyi/codex-useage-win/stargazers)

## What This Fork Shows

CodexUsageBar reads the current Codex account quota and local Codex Desktop **usage metadata**, so one widget can show quota, tokens, and API-equivalent value.

### Remote quota

- `5H` remaining quota, only when the backend actually exposes a five-hour lane
- `Week` remaining percentage for the current weekly quota cycle
- The upstream reset time / pace / reset-credit controls remain available in Standard mode

### Local tokens and API-equivalent cost

Local accounting uses Codex-reported usage metadata; it does not estimate tokens from prompt/output text length.

- `Task`: cumulative tokens + API-equivalent cost for the current/latest local Codex task
- `Last`: most recent available turn usage for the current task
- `Today`: tokens + API-equivalent cost for the current **Windows local calendar day**
- `Weekly spend`: API-equivalent cost from the remote weekly quota-cycle start through now, not Monday-Sunday
- `Lifetime spend`: cumulative discoverable local Codex session history and API-equivalent cost

API-equivalent cost is calculated by model and token category using supported public pricing for uncached input, cached input, cache-write input, and output where applicable. It answers “roughly what would the same token usage cost at API list prices?” and is **not your ChatGPT / Codex subscription bill**.

Display semantics:

- `≈$X.XX`: the scope is fully priceable with the currently supported pricing table
- `≥$X.XX`: at least this much is known; some usage in the scope cannot be priced reliably and is not silently treated as `$0`

## Screenshots / Layouts

### Standard Mode

Keeps account, plan, weekly quota, reset credits, and refresh controls, and adds a separate local-usage section for `Task` / `Last` / `Today` / weekly spend / lifetime spend.

![CodexUsageBar standard mode](IMG/3.png)

### Simple Mode

A compact movable summary with only the highest-value cards: optional `5H`, `Week`, weekly spend, and lifetime spend.

Current accepted layout example:

```text
┌──────────┐  ┌────────────────┐
│   Week   │  │  Weekly spend  │
│   82%    │  │    ≥$28.80     │
└──────────┘  └────────────────┘
┌──────────────────────────────┐
│        Lifetime spend        │
│           ≥$209.75           │
└──────────────────────────────┘
```

> The Owner supplied current Simple and Taskbar screenshots. The current GitHub connector cannot commit binary PNG data directly from chat attachments, so the repository image files have not been replaced in this docs change.

### Taskbar Mode

Docked to the current monitor's taskbar edge, with content-driven width and independent rounded cards. The complete 5H card disappears when the backend does not expose that lane.

Current accepted layout example:

```text
[ Week 82% ]  [ Weekly spend ≥$28.80 ]  [ Lifetime spend ≥$209.75 ]
```

## Features

- Native `Win32 + Direct2D + DirectWrite + WinHTTP`
- No `C#`, no `WebView`, no Electron
- Reads `%USERPROFILE%\.codex\auth.json` or `%CODEX_HOME%\auth.json`
- Requests `GET https://chatgpt.com/backend-api/wham/usage`
- Reads local usage metadata under `%USERPROFILE%\.codex\sessions` / `%CODEX_HOME%\sessions`
- Three display modes:
  - **Standard:** account / plan / quota / reset-credit controls plus Task / Last / Today / weekly spend / lifetime spend
  - **Simple:** optional 5H + Week + weekly spend + lifetime spend in a materially smaller movable widget
  - **Taskbar:** the same high-value summary as a tightly wrapped docked card row
- Cumulative session snapshots are deduplicated instead of being summed repeatedly
- Model attribution uses recognized canonical Codex rollout metadata rather than arbitrary nested `model` fields
- Light / Dark theme support and Windows DPI scaling
- Desktop overlay with drag and resize support (Taskbar mode remains docked)
- Launch-at-startup, Always on top, and Lock position toggles
- English / Chinese UI switch

## Privacy and Security

- `auth.json`, `access_token`, `refresh_token`, and `id_token` are not written into README files, test fixtures, or ordinary logs
- The local session parser extracts only the structures needed for usage/model accounting; prompts, assistant content, code/tool content are not displayed, logged, or uploaded by this feature
- Automated tests use synthetic / redacted fixtures rather than real Codex sessions or credentials
- API-equivalent cost is computed locally from usage statistics and does not require another API key

## Refresh Behavior

- Remote quota API: default refresh every `60` seconds
- Local token / API-equivalent statistics: background rescan about every `5` minutes, with an immediate initial scan at app startup
- Local countdown / UI repaint: every `1` second
- Right-click `Refresh now`: refreshes remote quota immediately
- OAuth: current code supports proactive refresh near expiry, recovery refresh after 401 / 403, and a manual `Refresh Token` menu action

## Usage

- Drag the widget body to move it
- Drag the right edge, bottom edge, or bottom-right corner to resize it
- Taskbar mode stays docked and does not support dragging or resizing
- Right-click menu includes:
  - `Refresh now`
  - `Refresh Token`
  - `Launch at startup`
  - `Always on top`
  - `Lock position`
  - `Display mode`
  - `Language`
  - `Reset widget position`
  - `Exit`

Position and size are stored in:

- `%APPDATA%\CodexUsageBar\settings.ini`

Startup registration uses the current-user registry key:

- `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`

## Build Locally

### Direct Build

```cmd
build.cmd
```

Output:

- `CodexUsageBar.exe`

### Build with CMake

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The project also includes CTest coverage for local token accounting, pricing, and presentation behavior.

## GitHub Actions

The workflow supports:

- `x64` / `ARM64` builds
- `pull_request`
- push to `main` / `master`
- `workflow_dispatch`
- `v*` tags triggering a public GitHub Release

> Important: a `v*` tag triggers a public Release. This fork currently has no public Release. Verify upstream licensing/authorization and the current release decision before tagging or redistributing binaries.

## Known Limitations

- `chatgpt.com/backend-api/wham/usage` and its fields are not a stable public contract controlled by this project; backend changes may require parser updates
- Local accounting depends on the Codex session JSONL / rollout schema and may require adaptation if Codex changes that schema
- API-equivalent cost depends on the maintained pricing coverage; historical, third-party, or internal models can produce a `≥$...` lower-bound estimate
- Lifetime totals include only currently discoverable/readable local session history and are not an OpenAI account-side billing history
- This is a desktop overlay widget, not the legacy Windows Gadget platform
- Upstream currently does not declare an explicit LICENSE; verify authorization / licensing requirements before publicly redistributing fork binaries

## Star History

<a href="https://www.star-history.com/?repos=luodaoyi%2Fcodex-useage-win&type=timeline&logscale=&legend=top-left">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=luodaoyi/codex-useage-win&type=timeline&theme=dark&logscale&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=luodaoyi/codex-useage-win&type=timeline&logscale&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/chart?repos=luodaoyi/codex-useage-win&type=timeline&logscale&legend=top-left" />
 </picture>
</a>
