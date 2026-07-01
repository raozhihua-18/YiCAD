# YiCAD Demo 插件

该插件只依赖 `YiCAD::PluginSdk` 公开接口目标，不链接 Qt 或 YiCAD 内部库。它注册命令 `com.yicad.demo/demo.add-line` 和 Ribbon 中的 **Demo > Draw > Add demo line** 按钮。执行命令会重新获取当前文档，添加一条从 `(0, 0)` 到 `(100, 100)` 的直线，然后重生成并自动缩放视图。没有活动文档时，命令只显示提示消息。

## 构建

按仓库根目录的常规流程配置后，可以单独构建 Demo 目标：

```powershell
cmake --build --preset Release --target YiCadDemoPlugin
```

在当前预设下，DLL 位于 `build/Release/bin/YiCadDemoPlugin.dll`。Debug 预设对应 `build/Debug/bin/YiCadDemoPlugin.dll`。

## 绝对路径部署

以管理员身份打开 PowerShell，由用户创建真实清单。将占位路径替换为已构建 DLL 的绝对路径：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<plugin dll="D:\path\to\YiCadDemoPlugin.dll"/>
```

将内容保存为 `C:\ProgramData\YiCAD\plugins\demo.xml`。DLL 可以位于清单目录之外。

## 相对路径部署

1. 创建 `C:\ProgramData\YiCAD\plugins\demo\`。
2. 把 `YiCadDemoPlugin.dll` 复制到该子目录。
3. 把 [`demo.xml.example`](demo.xml.example) 复制为 `C:\ProgramData\YiCAD\plugins\demo.xml`。

清单中的相对 DLL 路径以 XML 所在目录为基准。YiCAD 只扫描 `C:\ProgramData\YiCAD\plugins` 第一层的 `*.xml`；其他目录中的相同清单不会被发现。

## 手工验收

1. 启动安装后的 YiCAD，确认 Demo Ribbon 按钮出现。
2. 无活动文档时点击按钮，确认应用显示提示且不崩溃。
3. 新建或打开文档后再次点击，确认直线出现、视图刷新并可撤销。
4. 分别使用绝对路径和相对路径清单验证加载。
5. 将同一 XML 放到其他目录，确认 YiCAD 不会扫描它。
