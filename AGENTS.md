# Project AGENTS — codex-useage-win

Repository: `itachy1986/codex-useage-win`

## 项目特有执行规则

1. 当前稳定分支为 `master`；写任务按当前 Issue 使用独立 Task Branch，PR 默认提交到 `master`；不得自行直接写入、Merge 或 Push 版本 Tag 到 `master`。
2. 本仓库是 `luodaoyi/codex-useage-win` 的 Fork。未经当前 Issue 明确授权，不得自动同步 upstream、覆盖本 Fork 修改或把上游代码/提交描述为本 Fork 原创；上游同步必须先比较差异再按合同执行。
3. 真实 `%USERPROFILE%\.codex\auth.json`、`%CODEX_HOME%\auth.json`、`access_token`、`refresh_token`、`id_token`、Cookie、Authorization 或完整 JWT 不得进入Git、Issue、PR、测试fixture、截图或普通日志；测试默认使用 synthetic / redacted 数据。
4. 涉及 Token读取/写回、OAuth刷新、`chatgpt.com/backend-api/wham/usage`、401/403恢复或其他账号凭据行为时，只按当前Issue授权修改，并遵守其 `security-threat-model` / 安全门禁；不得为了调试输出真实凭据。
5. 本项目按 Windows 原生 `Win32/C++` 事实验证；构建优先使用当前 `build.cmd` 或 CMake/MSBuild入口。构建产物和临时文件保持在Git外或gitignored位置，不把本机运行配置、用户设置或账号文件提交到仓库。
6. 版本 `v*` Tag 会触发公开 GitHub Release；除非当前任务已取得明确发布授权，不得创建/Push版本Tag、发布Release或修改发布流程来绕过该门禁。
7. 涉及 `%APPDATA%\CodexUsageBar\settings.ini` 或 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 的任务只允许操作当前用户范围，并保持可撤销/可恢复；不得扩展到系统级注册表或其他用户范围。