# LazyEnv

[English](README.md) | **中文**

**跨平台、可恢复、零污染的问卷式开发环境配置工具。**

版本: 0.1.0

---

## 概览

LazyEnv 是一款原生 Windows 桌面应用程序，通过交互式的问卷界面引导开发者配置其开发环境。它利用 WebView2 提供符合 Windows 11 Fluent Design 的现代化用户界面，同时提供环境变量管理功能，并具备完整的快照与回滚能力。

### 核心设计原则

| 原则 | 描述 |
|-----------|-------------|
| **跨平台** | 抽象架构与特定平台后端分离；通过 Win32 + WebView2 提供 Windows 原生支持 (P0) |
| **可恢复** | 完整的环境变量快照与恢复系统；安装前自动创建快照 (P1) |
| **零污染** | 所有更改均被追踪且可逆；对宿主系统没有隐藏的副作用 |

---

## 架构

```
LazyEnv/
|-- main.cpp                 // WinMain 入口，窗口创建，消息分发
|-- webview_host.cpp/h       // WebView2 宿主管理
|-- installer.cpp/h          // 包安装 (winget)，PATH 路径管理
|-- rollback.cpp/h           // 环境变量快照与回滚引擎
|-- resources/
|   |-- index.html           // 问卷界面结构
|   |-- style.css            // Win11 Fluent Design 样式
|   |-- script.js            // 前端交互逻辑
|   |-- i18n.js              // 国际化语言包与逻辑
|-- LazyEnv.rc               // 图标与版本信息
|-- CMakeLists.txt           // 构建配置
```

### 通信模型

应用程序在原生 C++ 后端和 WebView2 前端之间使用双向消息桥接：

- **Web 到 Native**: `window.chrome.webview.postMessage(JSON.stringify(obj))`
- **Native 到 Web**: `ICoreWebView2::PostWebMessageAsString(json)`

所有消息均使用 JSON 编码，并包含用于路由的 `action` 字段。

---

## 功能特性

### Windows 原生支持

- 内嵌 WebView2 浏览器的 Win32 窗口
- 匹配 Windows 11 系统外观的暗色主题
- 无边框窗口，支持自定义标题栏、拖拽、最大化/最小化
- DPI 感知与字体抗锯齿优化

### 环境变量管理

- 通过 Windows 注册表 API 读写环境变量
- 支持用户级和系统级变量
- 为已安装的包自动管理 PATH
- 广播 `WM_SETTINGCHANGE` 消息使更改立即生效

### 容错与恢复

- 任何安装操作前自动创建快照
- 支持手动创建带描述的快照
- 可从任意快照完整恢复环境变量
- 快照以 JSON 格式存储在 `%LOCALAPPDATA%/LazyEnv/snapshots/` 中
- 通过 UI 查看、恢复和删除快照

### 跨平台一致性抽象

- 基于命名空间的模块隔离 (`lazyenv::`)
- 平台无关的接口设计 (`Installer`, `RollbackManager`)
- 后端可替换为 Linux/macOS 实现
- 前端完全跨平台 (HTML/CSS/JS)

---

## 软件包目录

LazyEnv 预置了常用开发工具的精选目录，并支持自动扫描本机已安装环境：

| 类别 | 软件包 |
|----------|----------|
| 编程语言 | Python 3.12, Node.js LTS, Rust, Go, Java JDK 21 |
| 构建工具 | CMake, Ninja, LLVM/Clang |
| 版本控制 | Git, GitHub CLI |
| 编辑器 | VS Code, Neovim |
| 容器 | Docker Desktop |
| 数据库 | PostgreSQL 16, Redis |
| 实用工具 | jq, ripgrep, fd, fzf, WinSCP |

---

## 构建指南

### 前置要求

- Windows 10 版本 1903+ 或 Windows 11
- 带有 C++ 桌面开发工作负载的 Visual Studio 2022/2026
- CMake 3.24+
- Microsoft Edge WebView2 运行时 (Windows 10/11 通常已预装)

### 构建步骤

```powershell
# 克隆或导航到项目目录
cd E:\Dev\LazyEnv

# 配置 (CMake 会自动下载 WebView2 SDK 和 WIL)
cmake -B build -G "Visual Studio 18 2026" -A x64

# 构建
cmake --build build --config Release

# 可执行文件将生成在 build/Release/LazyEnv.exe
# 资源文件会自动复制到 build/Release/resources/
```

---

## 使用方法

1. **启动** `LazyEnv.exe`
2. **主页** - 自动扫描并展示本机已安装的开发环境，支持手动添加和卸载
3. **系统检查** - 验证操作系统、WebView2 运行时和 winget 可用性
4. **选择软件包** - 从分类目录中选择所需的开发工具
5. **安装** - 通过 winget 安装软件包并实时显示日志和进度；安装开始前会自动创建快照，失败支持一键重试
6. **恢复** - 查看、创建、恢复或删除环境快照
7. **总结** - 回顾会话期间所做的所有更改

---

## 数据存储

| 项目 | 位置 |
|------|----------|
| 快照 | `%LOCALAPPDATA%/LazyEnv/snapshots/*.json` |
| WebView2 缓存 | `%LOCALAPPDATA%/LazyEnv/WebView2/` |

除了上述目录和 Windows 注册表环境变量键之外，不会在系统其他位置创建或修改任何文件。

---

## 许可证

MIT License
