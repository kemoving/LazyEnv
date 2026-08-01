# LazyEnv — Project Memory

## 项目定位
跨平台、可恢复、零污染的问卷式开发环境配置工具（Windows 原生桌面应用，v0.6.1）。

## 技术栈
- **语言**: C++17/20 (后端) + HTML/CSS/JS (前端)
- **UI 框架**: Win32 无边框窗口 + WebView2 (Win11 Fluent Design 暗色主题)
- **构建系统**: CMake 3.24+, Visual Studio 2022/2026
- **包管理**: winget (Windows Package Manager)
- **依赖**: WebView2 SDK (CMake 自动拉取), WIL
- **许可证**: GPL v3 (源码), MIT (readme 中标注)

## 架构概览
```
main.cpp          — WinMain 入口, 窗口创建, 消息分发, WndProc (SEH 防护)
webview_host.cpp/h — WebView2 宿主管理, 双向消息桥接
installer.cpp/h   — winget 包安装, PATH 管理, 环境检测
rollback.cpp/h    — 环境变量快照/回滚引擎 (JSON 存储)
resources/        — index.html, style.css, script.js, i18n.js
```

## 通信模型
- **Web → Native**: `window.chrome.webview.postMessage(JSON.stringify(obj))`
- **Native → Web**: `ICoreWebView2::PostWebMessageAsString(json)`
- 所有消息 JSON 编码, 含 `action` 字段路由

## 核心功能
1. **环境检测**: 启动时自动扫描 40+ 已安装开发工具 (Python/Node/Go/Rust/Java/...)
2. **软件包目录**: 精选 15+ 开发工具, 按类别 (语言/构建/版本控制/编辑器/容器/数据库/工具)
3. **安装引擎**: 通过 winget 安装, 实时日志流式输出, 失败支持一键重试
4. **快照恢复**: 任意安装前自动创建快照, 支持手动创建/删除/导出/导入/增量恢复/差异对比
5. **环境变量管理**: 读写注册表 (用户级+系统级), PATH 自动管理, `WM_SETTINGCHANGE` 广播
6. **设置页**: 直接编辑/删除环境变量, 自动快照保护
7. **国际化**: i18n.js 支持中文/英文

## 数据存储
- 快照: `%LOCALAPPDATA%/LazyEnv/snapshots/*.json`
- WebView2 缓存: `%LOCALAPPDATA%/LazyEnv/WebView2/`
- 除此之外零污染

## 韧性与安全设计
- **SEH 三层防护**: VEH (最先) → __try/__except (中层) → UnhandledExceptionFilter (最后)
- **线程安全**: 所有后台工作通过 SEH-safe `CreateThread` + `launchThreadSafe` 执行
- **WebView2 消息**: 通过自定义 `WM_WEBVIEW_POST_MESSAGE` 实现跨线程安全投递
- **无边框窗口**: `WS_POPUP` + DWM 阴影, 自定义最大化(避免 WebView2 GPU 纹理裁剪), DPI 感知 v2
- **最小窗口**: 800x600

## 关键命名空间/类
- `lazyenv::WebViewHost` — WebView2 生命周期管理
- `lazyenv::Installer` — 静态方法: `isWingetAvailable`, `isPackageInstalled`, `runCommand`, `runCommandStreaming`, `addToUserPath`
- `lazyenv::RollbackManager` — `createSnapshot`, `listSnapshots`, `restoreSnapshot`, `restoreSnapshotIncremental`, `deleteSnapshot`, `diffSnapshot`, `exportSnapshot`, `importSnapshot`, `readEnvVariable`, `writeEnvVariable`, `deleteEnvVariable`, `broadcastEnvironmentChange`
- `lazyenv::getDefaultCatalog()` — 返回 `vector<PackageInfo>` 定义软件包目录
- `lazyenv::WM_WEBVIEW_POST_MESSAGE` / `lazyenv::WM_WEBVIEW_DRAG_START` — 自定义窗口消息

## 版本发布
- Git tag `v*` 触发 GitHub Actions CI → 构建 + 打包 zip + 分类 Changelog + GitHub Release
- 当前版本: v0.6.1

## 代码位置
- 主仓库: `https://github.com/kemoving/LazyEnv`



## 用户偏好
- 不创建日期命名的 memory 文件（如 `YYYY-MM-DD.md`）不要在写入 MEMORY.md  ，后续都不记录了
