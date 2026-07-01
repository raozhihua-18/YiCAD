/*
 * Copyright (C) 2024-2026 YiCAD Contributors
 *
 * This file is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/// @file DemoPlugin.cpp
/// @brief 演示通过公开 SDK 注册命令并向当前文档添加直线

#include "YiCadPluginSdk.h"

namespace
{

constexpr const char* PluginId = "com.yicad.demo";
constexpr const char* CommandId = "demo.add-line";

class DemoPlugin
{
public:
    bool initialize(
        const YiCadHostApi* api,
        YiCadPluginApi* plugin) noexcept
    {
        yicad::plugin::Host host(api);
        if (!host || plugin == nullptr ||
            plugin->structSize < sizeof(YiCadPluginApi) ||
            plugin->abiVersion != YICAD_PLUGIN_ABI_VERSION)
        {
            return false;
        }

        m_host = host;
        plugin->pluginId = PluginId;
        plugin->pluginName = "YiCAD Demo Plugin";
        plugin->pluginVersion = "1.0.0";

        const bool commandRegistered = m_host.registerCommand(
            PluginId,
            CommandId,
            "Add demo line",
            &DemoPlugin::executeCommand,
            this);
        const bool ribbonRegistered = m_host.registerRibbonButton(
            PluginId,
            "Demo",
            "Draw",
            CommandId,
            "");
        return commandRegistered && ribbonRegistered;
    }

    void shutdown() noexcept
    {
        m_host = yicad::plugin::Host();
    }

private:
    static void YICAD_PLUGIN_CALL executeCommand(void* userData) noexcept
    {
        auto* plugin = static_cast<DemoPlugin*>(userData);
        if (plugin != nullptr)
        {
            plugin->addDemoLine();
        }
    }

    void addDemoLine() noexcept
    {
        const auto document = m_host.currentDocument();
        if (!document)
        {
            m_host.message("No active document.");
            return;
        }

        if (!document.addLine(0.0, 0.0, 100.0, 100.0))
        {
            m_host.message("Could not add the demo line.");
            return;
        }

        document.regen();
        document.zoomAuto();
    }

    yicad::plugin::Host m_host;
};

DemoPlugin g_plugin;

} // namespace

YICAD_PLUGIN_EXPORT uint32_t YICAD_PLUGIN_CALL
yicad_plugin_get_abi_version(void)
{
    return YICAD_PLUGIN_ABI_VERSION;
}

YICAD_PLUGIN_EXPORT YiCadResult YICAD_PLUGIN_CALL
yicad_plugin_init(const YiCadHostApi* host, YiCadPluginApi* plugin)
{
    try
    {
        return g_plugin.initialize(host, plugin)
            ? YICAD_SUCCESS
            : YICAD_FAILURE;
    }
    catch (...)
    {
        return YICAD_FAILURE;
    }
}

YICAD_PLUGIN_EXPORT void YICAD_PLUGIN_CALL
yicad_plugin_shutdown(void)
{
    try
    {
        g_plugin.shutdown();
    }
    catch (...)
    {
    }
}
