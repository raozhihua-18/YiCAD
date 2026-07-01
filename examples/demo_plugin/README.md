# YiCAD Demo 插件

该插件只依赖 `YiCAD::PluginSdk` 公开接口目标，不链接 Qt 或 YiCAD 内部库。它注册命令 `com.yicad.demo/demo.add-line`、Ribbon 中的 **Demo > Draw > Add demo line** 按钮，以及 `.demo` 导入和 `com.yicad.demo/demo` 导出格式。

执行命令会重新获取当前文档，添加一条从 `(0, 0)` 到 `(100, 100)` 的直线，然后重生成并自动缩放视图。ABI v2 的 `.demo` 导入会在一个文档事务中批量添加直线和圆：全部解析成功后一次提交，任意记录失败则整体回滚。选择 **YiCAD Demo Drawing (*.demo)** 导出时，插件通过只读实体迭代 API 输出当前文档中的真实直线和圆数据。

## Demo 文件格式

文件使用无 BOM 的 UTF-8 文本。首行固定为 `YICAD_DEMO_V2`，后续每行是一条实体记录：

```text
YICAD_DEMO_V2
LINE 0 0 100 100
CIRCLE 50 50 25
```

- `LINE` 后依次为起点 `x y` 和终点 `x y`。
- `CIRCLE` 后依次为圆心 `x y` 和半径。
- 空行会被忽略；未知类型、缺少参数、多余参数或无效几何会使整个导入失败并回滚。
- 当前只读 ABI 仅定义直线和圆数据，导出时其他实体类型不会写入 `.demo` 文件。

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

相对路径和绝对路径都是受支持的部署方式。只要最终解析到同一个 DLL，加载效果相同。开发时可让清单直接指向仓库 `build/<config>/bin` 下的绝对路径；固定部署建议使用 `demo/YiCadDemoPlugin.dll`，避免依赖本机构建目录。不要同时部署两个指向不同 Demo DLL 的清单，否则相同插件 ID 和格式注册会发生冲突。

## 手工验收

1. 启动安装后的 YiCAD，确认 Demo Ribbon 按钮出现。
2. 无活动文档时点击按钮，确认应用显示提示且不崩溃。
3. 新建或打开文档后再次点击，确认直线出现、视图刷新并可撤销。
4. 打开包含多条 `LINE`/`CIRCLE` 记录的有效 `.demo` 文件，确认实体全部出现；执行一次撤销，确认本次导入的所有实体同时消失。
5. 打开先包含有效记录、后包含无效记录的 `.demo` 文件，确认导入失败且没有留下已解析的部分实体。
6. 将包含直线和圆的文档另存为 `.demo`，确认文件包含真实坐标数据，并可重新导入得到相同的直线和圆。
7. 分别使用绝对路径和相对路径清单验证加载。
8. 将同一 XML 放到其他目录，确认 YiCAD 不会扫描它。
