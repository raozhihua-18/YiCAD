# YiCAD 插件机制设计方案

## 背景与目标

本方案设计一套新的 YiCAD 插件机制，不依赖现有旧插件接口。插件通过 XML 清单文件注册。主程序启动时只扫描 `C:\ProgramData\YiCAD\plugins` 目录中的 XML 文件，读取插件声明的 DLL 路径，然后加载并初始化插件。

第一版目标：

- 只从 `C:\ProgramData\YiCAD\plugins` 读取 XML 插件清单。
- DLL 路径由各插件在自己的 XML 中声明，不要求 DLL 位于 XML 目录内。
- 通过稳定 ABI 加载原生插件。
- 提供建立在 C ABI 之上的官方 C++ SDK 类封装。
- 插件可以注册命令、Ribbon 按钮、文件导入导出能力。
- 插件可以通过受控 API 操作文档。
- 不设计插件加载日志。
- 不设计插件管理 UI。
- 不设计插件禁用配置。

## 总体架构

插件机制分为四层：

1. 插件发现层
   - 只扫描 `C:\ProgramData\YiCAD\plugins` 目录下的 XML 文件。
   - XML 只提供插件 DLL 路径；插件 ID、名称、版本和 ABI 版本由 DLL 导出接口提供。

2. 插件运行时层
   - 加载插件动态库。
   - 查找插件导出函数。
   - 调用插件初始化和关闭函数。
   - 保存插件生命周期状态。

3. 插件 ABI 层
   - 定义稳定的 C ABI。
   - 插件和主程序通过函数表交互。
   - 官方 C++ SDK 在 C ABI 之上提供类封装，类只在插件内部使用，不作为动态库边界。
   - 避免直接暴露 C++ 类、Qt 对象、内部数据结构。

4. 宿主服务层
   - 主程序向插件提供命令注册、Ribbon 注册、文件过滤器注册、文档操作等能力。
   - 插件不直接访问 YiCAD 内部对象，只通过 opaque handle 和函数表调用。

## 目录规范

YiCAD 只使用一个固定的插件清单目录：

```text
C:\ProgramData\YiCAD\
  plugins\
    demo.xml
    vendor-a.xml
```

约束如下：

- 主程序不扫描其他系统级或用户级目录。
- `plugins` 目录只承担插件发现入口的职责，不规定 DLL、图标、资源和配置文件的存放位置。
- 每个插件自行组织其文件，并在 XML 中声明 DLL 路径。
- 安装或卸载插件，本质上是在该目录中添加或移除对应 XML 文件。

## XML 清单格式

XML 示例：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<plugin dll="D:\YiCADPlugins\DemoPlugin\DemoPlugin.dll"/>
```

字段说明：

| 字段 | 含义 |
| --- | --- |
| `dll` | 插件 DLL 路径，唯一的必填字段。可以是绝对路径，也可以是相对 XML 文件所在目录的相对路径。 |

设计约束：

- 一个 XML 文件只声明一个插件 DLL。
- XML 不保存插件 ID、名称、版本、ABI、兼容性、平台、架构、编译配置、按钮、菜单或命令等信息。
- 插件 ID、名称和版本由插件初始化时返回；ABI 版本通过导出函数查询。
- 按钮、菜单、命令和导入导出能力由插件初始化时通过宿主 API 注册。
- 相对路径以 XML 文件所在目录为基准；主程序解析后必须将其规范化为绝对路径。
- 主程序不展开路径中的环境变量，避免不同运行环境产生不同结果。

## 启动加载流程

主程序启动后，在主窗口创建和基础系统初始化完成后加载插件：

1. 创建 `PluginManager`。
2. 扫描 `C:\ProgramData\YiCAD\plugins\*.xml`，不递归扫描子目录。
3. 对每个 XML 文件调用 `PluginManifestReader`。
4. 验证根节点为 `<plugin>`，且 `dll` 属性非空。
5. 解析并规范化 DLL 路径，检查目标是存在的 `.dll` 文件。
6. 按 XML 文件名排序，确保加载顺序确定且可复现。
7. 对每个清单调用 `NativePluginLoader`。
8. 加载动态库。
9. 查找导出函数：
   - `yicad_plugin_get_abi_version`
   - `yicad_plugin_init`
   - `yicad_plugin_shutdown`
10. 调用 `yicad_plugin_get_abi_version` 检查 ABI 版本。
11. 构造宿主 API 函数表。
12. 调用 `yicad_plugin_init`。
13. 校验插件返回的 ID 非空且未重复，并记录名称和版本。
14. 插件通过宿主 API 注册命令、Ribbon、导入导出过滤器等能力。
15. 主程序把注册结果接入现有命令系统、Ribbon 和文件 IO。

第一版不做热加载、热卸载。插件在程序启动时加载，程序退出时统一调用关闭函数。

## 建议新增模块

建议新增目录：

```text
YiCAD/src/plugin_runtime/
  PluginManifest.h
  PluginManifest.cpp
  PluginManifestReader.h
  PluginManifestReader.cpp
  PluginManager.h
  PluginManager.cpp
  NativePluginLoader.h
  NativePluginLoader.cpp
  PluginRegistry.h
  PluginRegistry.cpp
  YiCadPluginAbi.h
  YiCadPluginSdk.h
  HostApi.h
  HostApi.cpp
```

职责划分：

| 模块 | 职责 |
| --- | --- |
| `PluginManifest` | 保存 XML 文件路径和解析、规范化后的 DLL 路径。 |
| `PluginManifestReader` | 扫描并解析 XML。 |
| `PluginManager` | 管理发现、校验、加载、关闭流程。 |
| `NativePluginLoader` | 封装动态库加载和导出函数查找。 |
| `PluginRegistry` | 保存插件注册的命令、Ribbon 按钮、文件过滤器。 |
| `YiCadPluginAbi.h` | 对插件公开的稳定 ABI 头文件。 |
| `YiCadPluginSdk.h` | 官方 C++ 类封装，仅转发到 `YiCadPluginAbi.h` 的函数表。 |
| `HostApi` | 把 YiCAD 内部能力适配成插件可调用的函数表。 |

## 原生插件 ABI

插件动态库必须导出固定函数：

```cpp
extern "C" __declspec(dllexport)
int yicad_plugin_get_abi_version();

extern "C" __declspec(dllexport)
bool yicad_plugin_init(const YiCadHostApi* host, YiCadPluginApi* plugin);

extern "C" __declspec(dllexport)
void yicad_plugin_shutdown();
```

跨平台宏建议：

```cpp
#if defined(_WIN32)
#define YICAD_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define YICAD_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif
```

示例：

```cpp
YICAD_PLUGIN_EXPORT int yicad_plugin_get_abi_version()
{
    return YICAD_PLUGIN_ABI_VERSION;
}

YICAD_PLUGIN_EXPORT bool yicad_plugin_init(
    const YiCadHostApi* host,
    YiCadPluginApi* plugin)
{
    plugin->pluginId = "com.yicad.demo";
    plugin->pluginName = "Demo Plugin";
    plugin->pluginVersion = "1.0.0";

    host->registerCommand(
        "com.yicad.demo",
        "demo.hello",
        "Hello",
        &demo_hello_command,
        nullptr);

    host->registerRibbonButton(
        "com.yicad.demo",
        "Demo",
        "Tools",
        "demo.hello",
        "icons/demo.svg");

    return true;
}

YICAD_PLUGIN_EXPORT void yicad_plugin_shutdown()
{
}
```

## 宿主 API

第一版宿主 API 应保持克制，先提供最小能力。

```cpp
#define YICAD_PLUGIN_ABI_VERSION 1

typedef void* YiCadDocumentHandle;

typedef void (*YiCadCommandCallback)(void* userData);

struct YiCadHostApi {
    int abiVersion;

    void (*message)(const char* text);

    bool (*registerCommand)(
        const char* pluginId,
        const char* commandId,
        const char* displayName,
        YiCadCommandCallback callback,
        void* userData);

    bool (*registerRibbonButton)(
        const char* pluginId,
        const char* tab,
        const char* group,
        const char* commandId,
        const char* iconPath);

    YiCadDocumentHandle (*currentDocument)();

    bool (*documentAddLine)(
        YiCadDocumentHandle doc,
        double x1,
        double y1,
        double x2,
        double y2);

    bool (*documentAddCircle)(
        YiCadDocumentHandle doc,
        double cx,
        double cy,
        double radius);

    bool (*documentRegen)(YiCadDocumentHandle doc);
    bool (*documentZoomAuto)(YiCadDocumentHandle doc);
};

struct YiCadPluginApi {
    const char* pluginId;
    const char* pluginName;
    const char* pluginVersion;
};
```

注意：

- 插件只能保存 `pluginId`、`commandId` 这类字符串 ID。
- 插件不应长期保存文档 handle。命令执行时重新获取当前文档更安全。
- 字符串第一版建议统一 UTF-8。
- 所有函数返回 `bool` 表示是否成功，失败时插件自行决定是否提示用户。

## 官方 C++ SDK 类封装

C ABI 是 YiCAD 与插件之间唯一的动态库边界。官方 SDK 在其上提供轻量 C++ 类，让插件使用成员函数、对象作用域和 RAII 等 C++ 写法，同时不把 C++ 类布局、虚函数表、STL 或 Qt 类型暴露给主程序。

建议将封装实现为 `YiCadPluginSdk.h` 中的内联代码。`Host` 保存 `YiCadHostApi` 指针，`Document` 保存 opaque handle；它们不拥有主程序对象，也不负责释放 handle。

```cpp
#pragma once

#include "YiCadPluginAbi.h"

namespace yicad::plugin
{

class Host;

class Document
{
public:
    Document() noexcept = default;

    explicit operator bool() const noexcept
    {
        return m_handle != nullptr;
    }

    bool addLine(
        double x1,
        double y1,
        double x2,
        double y2) const noexcept
    {
        return m_api && m_handle &&
               m_api->documentAddLine(m_handle, x1, y1, x2, y2);
    }

    bool addCircle(
        double cx,
        double cy,
        double radius) const noexcept
    {
        return m_api && m_handle &&
               m_api->documentAddCircle(m_handle, cx, cy, radius);
    }

    bool regen() const noexcept
    {
        return m_api && m_handle && m_api->documentRegen(m_handle);
    }

    bool zoomAuto() const noexcept
    {
        return m_api && m_handle && m_api->documentZoomAuto(m_handle);
    }

private:
    friend class Host;

    Document(
        const YiCadHostApi* api,
        YiCadDocumentHandle handle) noexcept
        : m_api(api),
          m_handle(handle)
    {
    }

    const YiCadHostApi* m_api = nullptr;
    YiCadDocumentHandle m_handle = nullptr;
};

class Host
{
public:
    explicit Host(const YiCadHostApi* api = nullptr) noexcept
        : m_api(api)
    {
    }

    explicit operator bool() const noexcept
    {
        return m_api != nullptr;
    }

    void message(const char* text) const noexcept
    {
        if (m_api && m_api->message)
        {
            m_api->message(text);
        }
    }

    bool registerCommand(
        const char* pluginId,
        const char* commandId,
        const char* displayName,
        YiCadCommandCallback callback,
        void* userData) const noexcept
    {
        return m_api && m_api->registerCommand &&
               m_api->registerCommand(
                   pluginId,
                   commandId,
                   displayName,
                   callback,
                   userData);
    }

    bool registerRibbonButton(
        const char* pluginId,
        const char* tab,
        const char* group,
        const char* commandId,
        const char* iconPath) const noexcept
    {
        return m_api && m_api->registerRibbonButton &&
               m_api->registerRibbonButton(
                   pluginId,
                   tab,
                   group,
                   commandId,
                   iconPath);
    }

    Document currentDocument() const noexcept
    {
        if (!m_api || !m_api->currentDocument)
        {
            return {};
        }
        return Document(m_api, m_api->currentDocument());
    }

private:
    const YiCadHostApi* m_api = nullptr;
};

} // namespace yicad::plugin
```

插件可以用普通 C++ 类组织自己的状态和业务逻辑，但导出入口仍保持 C ABI。插件对象不返回给主程序，也不由主程序释放：

```cpp
#include "YiCadPluginSdk.h"

namespace
{

constexpr const char* PluginId = "com.yicad.demo";

class DemoPlugin
{
public:
    bool initialize(
        const YiCadHostApi* api,
        YiCadPluginApi* plugin) noexcept
    {
        if (!api || !plugin)
        {
            return false;
        }

        m_host = yicad::plugin::Host(api);

        plugin->pluginId = PluginId;
        plugin->pluginName = "Demo Plugin";
        plugin->pluginVersion = "1.0.0";

        const bool commandRegistered = m_host.registerCommand(
            PluginId,
            "demo.add-line",
            "Add demo line",
            &DemoPlugin::executeCommand,
            this);

        const bool buttonRegistered = m_host.registerRibbonButton(
            PluginId,
            "Demo",
            "Draw",
            "demo.add-line",
            "icons/demo.svg");

        return commandRegistered && buttonRegistered;
    }

    void shutdown() noexcept
    {
        m_host = yicad::plugin::Host();
    }

private:
    static void executeCommand(void* userData) noexcept
    {
        auto* self = static_cast<DemoPlugin*>(userData);
        if (self)
        {
            self->addDemoLine();
        }
    }

    void addDemoLine() noexcept
    {
        const auto document = m_host.currentDocument();
        if (!document)
        {
            m_host.message("No active document");
            return;
        }

        if (document.addLine(0.0, 0.0, 100.0, 100.0))
        {
            document.regen();
            document.zoomAuto();
        }
    }

    yicad::plugin::Host m_host;
};

DemoPlugin g_plugin;

} // namespace

YICAD_PLUGIN_EXPORT int yicad_plugin_get_abi_version()
{
    return YICAD_PLUGIN_ABI_VERSION;
}

YICAD_PLUGIN_EXPORT bool yicad_plugin_init(
    const YiCadHostApi* host,
    YiCadPluginApi* plugin)
{
    return g_plugin.initialize(host, plugin);
}

YICAD_PLUGIN_EXPORT void yicad_plugin_shutdown()
{
    g_plugin.shutdown();
}
```

封装层约束：

- `YiCadPluginAbi.h` 是兼容性基准，C++ 类只是便利接口。
- 所有封装方法都在插件侧执行，最终调用 `YiCadHostApi` 函数表。
- 不允许异常穿过 C ABI；入口函数和回调必须捕获内部异常或保证 `noexcept`。
- 不允许跨边界传递或释放 C++、STL、Qt 对象。
- 主程序必须保证传入的 `YiCadHostApi` 函数表在插件关闭前始终有效。
- `Host` 的有效期不得超过插件初始化至关闭阶段；`Document` 不应被长期保存。
- SDK 类可以随功能增长，但不能绕过 ABI 版本检查调用宿主未提供的函数。

## 命令与 Ribbon 注册

插件初始化时注册命令：

```cpp
host->registerCommand(
    "com.yicad.demo",
    "demo.hello",
    "Hello",
    &demo_hello_command,
    nullptr);
```

再注册 Ribbon 按钮：

```cpp
host->registerRibbonButton(
    "com.yicad.demo",
    "Demo",
    "Tools",
    "demo.hello",
    "icons/demo.svg");
```

主程序行为：

- `PluginRegistry` 保存命令 ID 到回调函数的映射。
- Ribbon 按钮点击后，通过 `PluginRegistry` 查找命令并调用回调。
- 如果 `tab` 或 `group` 不存在，则创建。
- 如果多个插件注册相同 `tab` 或 `group`，应复用已有 UI 容器。
- `commandId` 在同一个 `pluginId` 下必须唯一。

## 文档 API

插件不直接访问 `DmDocument*`。主程序只给 opaque handle：

```cpp
typedef void* YiCadDocumentHandle;
```

插件调用：

```cpp
YiCadDocumentHandle doc = host->currentDocument();
if (doc) {
    host->documentAddLine(doc, 0.0, 0.0, 100.0, 100.0);
    host->documentRegen(doc);
}
```

主程序内部把 `YiCadDocumentHandle` 转回内部文档对象，并调用真实实现。

这样做的好处：

- 内部文档模型可以继续重构。
- 插件不依赖 YiCAD 内部头文件。
- 插件 ABI 不受内部 C++ 类变化影响。
- 可以逐步扩展文档操作函数，而不是一次暴露所有能力。

第一版文档 API 建议只提供：

- 获取当前文档。
- 添加直线。
- 添加圆。
- 重生成文档。
- 自动缩放视图。

后续再按需求增加：

- 图层操作。
- 文字操作。
- 标注操作。
- 选择集操作。
- 块操作。
- 事务和撤销支持。

## 文件导入导出插件

导入导出能力也通过宿主 API 注册。

建议增加：

```cpp
typedef bool (*YiCadImportCallback)(
    YiCadDocumentHandle doc,
    const char* filePath,
    void* userData);

typedef bool (*YiCadExportCallback)(
    YiCadDocumentHandle doc,
    const char* filePath,
    void* userData);

bool (*registerImportFilter)(
    const char* pluginId,
    const char* formatId,
    const char* displayName,
    const char* extension,
    YiCadImportCallback callback,
    void* userData);

bool (*registerExportFilter)(
    const char* pluginId,
    const char* formatId,
    const char* displayName,
    const char* extension,
    YiCadExportCallback callback,
    void* userData);
```

示例：

```cpp
host->registerImportFilter(
    "com.yicad.demo",
    "demo-format",
    "Demo Format (*.demo)",
    "demo",
    &demo_import,
    nullptr);
```

主程序行为：

- 文件打开时，根据扩展名查找导入过滤器。
- 文件保存时，根据格式 ID 查找导出过滤器。
- 找到后调用对应插件回调。
- 插件通过文档 API 添加或读取实体。

## 路径与安全边界

原生插件是 DLL，加载后和主程序运行在同一进程，拥有当前进程权限。因此 XML 不是安全边界，XML 只是发现机制。

第一版建议至少做：

- 动态库路径必须规范化。
- 只接受固定目录第一层中的 `.xml` 文件，不递归扫描，也不扫描其他目录。
- XML 声明的目标必须是存在的 `.dll` 文件；DLL 可以位于插件自行选择的目录。
- DLL 架构必须与当前程序匹配；该检查由动态库加载结果和 ABI 版本检查完成，不写入 XML。
- 使用绝对路径和安全的 DLL 依赖搜索策略加载，避免当前工作目录参与依赖解析。

由于 XML 可以指向任意 DLL，`C:\ProgramData\YiCAD\plugins` 的写权限必须只授予受信任的安装程序或管理员。后续如需企业级可信校验，应使用代码签名或外部部署白名单，不增加 XML 字段。

## 生命周期

插件生命周期：

```text
Discovered -> Validated -> Loaded -> Initialized -> Active -> Shutdown
```

含义：

| 状态 | 含义 |
| --- | --- |
| `Discovered` | 从 XML 中发现插件。 |
| `Validated` | 清单和路径校验通过。 |
| `Loaded` | 动态库加载成功。 |
| `Initialized` | 插件初始化函数调用成功。 |
| `Active` | 插件注册的能力已接入主程序。 |
| `Shutdown` | 程序退出时已调用插件关闭函数。 |

第一版不支持运行中卸载插件。

原因：

- 插件可能注册命令回调。
- 插件可能创建 Qt 对象。
- 插件可能启动线程。
- 插件可能持有静态对象。
- 卸载动态库后，任何残留函数指针都会变成悬空指针。

启动加载、退出释放是更稳妥的第一版策略。

## 为什么不用 C++ ABI 作为插件边界

这里的 "C++ ABI 不稳定" 指的是：插件 DLL 和主程序如果直接通过 C++ 类、虚函数、模板、STL、Qt 类型等跨 DLL 交互，它们不仅要源码接口一致，还要二进制层面的规则完全一致。

ABI 是 Application Binary Interface，即应用二进制接口。它决定了编译后的二进制如何调用函数、如何传参、如何返回值、类对象内存如何布局、虚函数表如何排列、异常如何传播、运行时库如何分配和释放内存等。

直接把 C++ 类作为插件接口时，会遇到这些风险：

1. 编译器差异
   - MSVC、MinGW、Clang、GCC 的 C++ ABI 不完全兼容。
   - 同样的 C++ 类，在不同编译器下生成的符号名、虚表布局、异常处理机制都可能不同。

2. MSVC 版本差异
   - 不同 MSVC 工具集可能改变 STL 实现细节、运行时库行为、调试迭代器设置。
   - 插件用一个工具集编译，主程序用另一个工具集编译，可能能链接但运行时崩溃。

3. Debug 和 Release 差异
   - Debug 版 STL 和 Release 版 STL 的对象布局、检查逻辑、内存分配策略可能不同。
   - 如果插件 Debug、主程序 Release，跨边界传 `std::string`、`std::vector`、Qt 容器或由一边分配另一边释放的对象，都很危险。

4. 编译选项差异
   - 运行时库 `/MD`、`/MDd`、异常开关、RTTI 开关、结构体对齐、字符集等都可能影响 ABI。
   - 结构体对齐不同会导致同一个结构体在主程序和插件里大小不同。

5. Qt 版本差异
   - Qt 类型虽然源码层面稳定，但不同 Qt 版本、不同编译配置下不保证可以作为跨插件 ABI 边界的稳定二进制接口。
   - 直接跨 DLL 传 `QString`、`QList`、`QObject*`，会把插件和主程序绑定到同一个 Qt 版本和同一套编译配置。

6. 内存所有权风险
   - 如果插件里创建对象，主程序里释放对象，双方必须使用完全兼容的运行时库和分配器。
   - 一旦不一致，就可能出现堆损坏。

因此，新插件机制建议使用 C ABI：

- C 函数导出符号稳定。
- 不暴露 C++ 类布局。
- 不跨边界传 STL 或 Qt 容器。
- 复杂能力通过函数表和 opaque handle 表达。
- 内存分配和释放可以明确规定由哪一侧负责。

主程序内部仍然可以使用 C++ 和 Qt。只是插件边界使用 C ABI，把不稳定因素隔离在主程序内部。

## 实施步骤

第一阶段实现最小链路：

1. 新增 `YiCadPluginAbi.h`。
2. 新增只依赖 C ABI 的 `YiCadPluginSdk.h` 类封装。
3. 新增 `PluginManifest`。
4. 新增 `PluginManifestReader`，只读取 `C:\ProgramData\YiCAD\plugins\*.xml`。
5. 新增 `NativePluginLoader`，加载 DLL 并查找导出函数。
6. 新增 `PluginRegistry`，保存命令和 Ribbon 注册信息。
7. 新增 `PluginManager`，串联发现、校验、加载、初始化。
8. 在主窗口初始化完成后调用 `PluginManager::loadAll()`。
9. 把注册的 Ribbon 按钮接入现有 Ribbon。
10. 使用官方 C++ SDK 写一个示例插件，注册一个命令和一个按钮。
11. 点击按钮后输出命令行消息或添加一条直线。

第二阶段扩展能力：

1. 增加导入导出过滤器注册。
2. 接入现有文件打开和保存流程。
3. 增加更多文档 API。
4. 增加事务和撤销支持。
5. 整理插件 SDK 示例工程。

第三阶段增强稳定性：

1. 增加 ABI 版本协商。
2. 增加插件 SDK 的 ABI 兼容性测试。
3. 增加 DLL 代码签名或外部部署白名单校验。
4. 完善重复插件 ID 和部分初始化失败时的回滚处理。

## 推荐第一版验收标准

第一版完成后，应满足：

- `C:\ProgramData\YiCAD\plugins\demo.xml` 能被读取，其他目录中的 XML 不会被扫描。
- XML 中的绝对和相对 DLL 路径能正确解析。
- DLL 可以位于 `C:\ProgramData\YiCAD\plugins` 之外。
- 主程序能加载 DLL。
- 主程序能调用 `yicad_plugin_init`。
- 示例插件能通过 `Host` 和 `Document` 类调用宿主能力，动态库边界仍只使用 C ABI。
- 插件能注册一个命令。
- 插件能注册一个 Ribbon 按钮。
- 点击按钮后能执行插件命令。
- 插件命令能获取当前文档。
- 插件命令能添加一条直线或圆。
- 程序退出时能调用 `yicad_plugin_shutdown`。
