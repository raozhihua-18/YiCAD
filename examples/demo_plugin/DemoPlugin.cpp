/*
 * Copyright (C) 2024-2026 YiCAD Contributors
 *
 * This file is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/// @file DemoPlugin.cpp
/// @brief 演示命令、Ribbon 和 .demo 文件导入导出

#include "YiCadPluginSdk.h"

#include <filesystem>
#include <fstream>

namespace
{

constexpr const char* PluginId = "com.yicad.demo";
constexpr const char* CommandId = "demo.add-line";
constexpr const char* FormatId = "demo";

class DemoPlugin
{
public:
    bool initialize(
        const YiCadHostApi* api,
        YiCadPluginApi* plugin) noexcept
    {
        yicad::plugin::Host host(api);
        if (!host || plugin == nullptr ||
            plugin->structSize < YICAD_PLUGIN_API_V1_SIZE ||
            plugin->abiVersion < YICAD_PLUGIN_ABI_MIN_VERSION ||
            plugin->abiVersion > YICAD_PLUGIN_ABI_MAX_VERSION)
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
        const bool importRegistered = m_host.registerImportFilter(
            PluginId,
            FormatId,
            "YiCAD Demo Drawing",
            "demo",
            &DemoPlugin::importFile,
            this);
        const bool exportRegistered = m_host.registerExportFilter(
            PluginId,
            FormatId,
            "YiCAD Demo Drawing",
            "demo",
            &DemoPlugin::exportFile,
            this);
        return commandRegistered && ribbonRegistered && importRegistered &&
               exportRegistered;
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

    static YiCadResult YICAD_PLUGIN_CALL importFile(
        YiCadDocumentHandle handle,
        const char* filePath,
        void* userData) noexcept
    {
        try
        {
            auto* plugin = static_cast<DemoPlugin*>(userData);
            if (plugin == nullptr || filePath == nullptr ||
                *filePath == '\0')
            {
                return YICAD_FAILURE;
            }

            const auto document = plugin->m_host.document(handle);
            if (!document ||
                !document.addLine(0.0, 0.0, 100.0, 100.0))
            {
                return YICAD_FAILURE;
            }
            document.regen();
            return YICAD_SUCCESS;
        }
        catch (...)
        {
            return YICAD_FAILURE;
        }
    }

    static YiCadResult YICAD_PLUGIN_CALL exportFile(
        YiCadDocumentHandle handle,
        const char* filePath,
        void* userData) noexcept
    {
        try
        {
            auto* plugin = static_cast<DemoPlugin*>(userData);
            if (plugin == nullptr || filePath == nullptr ||
                *filePath == '\0' || !plugin->m_host.document(handle))
            {
                return YICAD_FAILURE;
            }

            std::ofstream output(
                std::filesystem::u8path(filePath),
                std::ios::binary | std::ios::trunc);
            if (!output)
            {
                return YICAD_FAILURE;
            }
            output << "YICAD_DEMO_V1\n";
            output.flush();
            return output ? YICAD_SUCCESS : YICAD_FAILURE;
        }
        catch (...)
        {
            return YICAD_FAILURE;
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
    return YICAD_PLUGIN_ABI_MAX_VERSION;
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
