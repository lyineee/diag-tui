# FuseDiag

基于 UDS + DoIP 协议的汽车诊断 TUI 工具。通过以太网 DoIP（ISO 13400）连接车辆 ECU，实现 ISO 14229（UDS）诊断服务，使用 [ftxui](https://github.com/ArthurSonzogni/FTXUI) 渲染终端界面。

## 功能

- **DoIP 发现与连接** — UDP 广播发现、TCP 路由激活（ISO 13400-2）
- **DTC 读取** — 按状态掩码读取故障码，详情描述查询
- **DID 读写** — 读取数据标识符，展开/折叠，轮询与图表展示
- **原始 UDS 发送** — 发送任意十六进制载荷，查看原始响应
- **会话管理** — 切换默认/扩展/编程会话，ECU 复位，TesterPresent
- **可配置** — 源地址、目标地址、ECU IP 通过设置面板调整

## 构建

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
```

### 运行

```bash
# TUI 应用程序
./build/fuse-diag

# 单元测试
./build/tests/fuse-diag-tests

# 本地 DoIP 测试服务器（端口 13400）
./build/test-doip-server

# E2E 测试（需要 Node.js）
npm install
npm test
```

### 依赖（自动拉取）

| 依赖 | 仓库 | 用途 |
|---|---|---|
| ftxui | ArthurSonzogni/FTXUI (v5.0.0) | TUI 框架 |
| doip-lib | langroodi/DoIP-Lib (master) | DoIP 序列化 (ISO 13400-2) |
| uds-c | openxc/uds-c (master) | UDS 类型 (DiagnosticRequest, NRC 枚举) |
| nlohmann_json | nlohmann/json (v3.11.3) | JSON 配置文件解析 |
| spdlog | gabime/spdlog (v1.13.0) | 文件日志 (fuse-diag.log) |
| googletest | google/googletest (v1.14.0) | 测试框架 |

## 架构

```
src/
├── main.cpp                 # 入口，ftxui 事件循环，快捷键
├── app/
│   └── App.h/cpp            # 全局状态 (AppState)，协调 DoIP + UDS
├── doip/
│   ├── DoipTypes.h          # EcuInfo, DoipMessage 结构体与枚举
│   └── DoipClient.h/cpp     # 基于 DoIP-Lib 的异步 TCP/UDP 客户端
├── uds/
│   ├── UdsTypes.h           # DidEntry, DtcInfo, DiagResponse
│   ├── UdsMessage.h/cpp     # UDS 请求构建 + 响应解析
│   ├── UdsClient.h/cpp      # UDS 服务封装（会话、DTC、DID 等）
│   ├── DidDatabase.h/cpp    # DID 元数据（JSON 配置）
│   └── DtcDatabase.h/cpp    # DTC 名称/描述查询
├── ui/
│   ├── StatusBar.h/cpp      # 顶栏：连接状态、SA/TA、会话
│   ├── NavBar.h/cpp         # 左侧导航菜单
│   ├── DtcPage.h/cpp        # DTC 列表 + 详情面板
│   ├── DidPage.h/cpp        # DID 展开列表 + 轮询控制
│   ├── DidItem.h/cpp        # 单个 DID 展开/折叠组件
│   ├── RawPage.h/cpp        # 十六进制原始消息发送/响应
│   ├── SessionManager.h/cpp # 会话切换、ECU 复位、TesterPresent
│   └── SettingsPage.h/cpp   # IP/SA/TA 配置，连接/断开
config/
├── did_database.json        # DID 元数据定义
└── dtc_database.json        # DTC 名称/描述数据库
tools/
└── test_server.cpp          # 本地测试用 DoIP 服务器 (UDP+TCP :13400)
```

## 快捷键

| 按键 | 操作 |
|---|---|
| `Tab` | 循环切换页面 |
| `F2` | 跳转到 DID 页面 |
| `F3` | 跳转到原始发送页面 |
| `F5` | 刷新 DTC 列表 |
| `F6` | 清除 DTC |
| `Escape` | 断开连接并退出 |

## 配置

编辑 `config/` 目录下的 JSON 文件：

- `did_database.json` — DID 定义（名称、描述、数据大小、可图表化标志、轮询间隔）
- `dtc_database.json` — DTC 码到可读名称/描述的映射

## E2E 测试

端到端 TUI 测试使用 [microsoft/tui-test](https://github.com/microsoft/tui-test)，在真实终端中启动 `fuse-diag` 并模拟按键操作。测试针对本地 `test-doip-server` 实例运行。

```bash
# 安装测试依赖（仅需一次）
npm install

# 运行全部 E2E 测试
npm test

# 仅运行 DTC 页面测试
npm run test:dtc
```

测试文件位于 `tests/e2e/`，使用 TypeScript 编写，API 为 `test.describe` / `test` / `expect`。

## 常见问题

- **端口被占用**：`fuser -k 13400/tcp 13400/udp` 释放端口
- **日志**：输出到 `fuse-diag.log`，非标准输出
- **测试服务器**：无需 root 权限（端口 13400 > 1024）

## 许可证

MIT
