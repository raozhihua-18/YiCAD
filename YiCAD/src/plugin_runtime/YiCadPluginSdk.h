#ifndef YICAD_PLUGIN_SDK_H
#define YICAD_PLUGIN_SDK_H

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
        return hasField(
            offsetof(YiCadHostApi, abiVersion),
            sizeof(m_api->abiVersion));
    }

    bool addLine(
        double x1,
        double y1,
        double x2,
        double y2) const noexcept
    {
        return hasField(
                   offsetof(YiCadHostApi, documentAddLine),
                   sizeof(m_api->documentAddLine)) &&
               m_api->documentAddLine != nullptr &&
               m_api->documentAddLine(m_handle, x1, y1, x2, y2) ==
                   YICAD_SUCCESS;
    }

    bool addCircle(
        double centerX,
        double centerY,
        double radius) const noexcept
    {
        return hasField(
                   offsetof(YiCadHostApi, documentAddCircle),
                   sizeof(m_api->documentAddCircle)) &&
               m_api->documentAddCircle != nullptr &&
               m_api->documentAddCircle(
                   m_handle,
                   centerX,
                   centerY,
                   radius) == YICAD_SUCCESS;
    }

    bool regen() const noexcept
    {
        return hasField(
                   offsetof(YiCadHostApi, documentRegen),
                   sizeof(m_api->documentRegen)) &&
               m_api->documentRegen != nullptr &&
               m_api->documentRegen(m_handle) == YICAD_SUCCESS;
    }

    bool zoomAuto() const noexcept
    {
        return hasField(
                   offsetof(YiCadHostApi, documentZoomAuto),
                   sizeof(m_api->documentZoomAuto)) &&
               m_api->documentZoomAuto != nullptr &&
               m_api->documentZoomAuto(m_handle) == YICAD_SUCCESS;
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

    bool hasField(size_t offset, size_t size) const noexcept
    {
        return m_api != nullptr && m_handle != nullptr &&
               m_api->structSize >=
                   offsetof(YiCadHostApi, abiVersion) +
                       sizeof(m_api->abiVersion) &&
               m_api->abiVersion >= YICAD_PLUGIN_ABI_MIN_VERSION &&
               m_api->abiVersion <= YICAD_PLUGIN_ABI_MAX_VERSION &&
               m_api->structSize >= offset + size;
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
        return isCompatible();
    }

    void message(const char* text) const noexcept
    {
        if (text != nullptr &&
            hasField(
                offsetof(YiCadHostApi, message),
                sizeof(m_api->message)) &&
            m_api->message != nullptr)
        {
            m_api->message(text);
        }
    }

    bool registerCommand(
        const char* pluginId,
        const char* commandId,
        const char* displayName,
        YiCadCommandCallback callback,
        void* userData = nullptr) const noexcept
    {
        return pluginId != nullptr && commandId != nullptr &&
               displayName != nullptr && callback != nullptr &&
               hasField(
                   offsetof(YiCadHostApi, registerCommand),
                   sizeof(m_api->registerCommand)) &&
               m_api->registerCommand != nullptr &&
               m_api->registerCommand(
                   pluginId,
                   commandId,
                   displayName,
                   callback,
                   userData) == YICAD_SUCCESS;
    }

    bool registerRibbonButton(
        const char* pluginId,
        const char* tab,
        const char* group,
        const char* commandId,
        const char* iconPath) const noexcept
    {
        return pluginId != nullptr && tab != nullptr && group != nullptr &&
               commandId != nullptr && iconPath != nullptr &&
               hasField(
                   offsetof(YiCadHostApi, registerRibbonButton),
                   sizeof(m_api->registerRibbonButton)) &&
               m_api->registerRibbonButton != nullptr &&
               m_api->registerRibbonButton(
                   pluginId,
                   tab,
                   group,
                   commandId,
                   iconPath) == YICAD_SUCCESS;
    }

    bool registerImportFilter(
        const char* pluginId,
        const char* formatId,
        const char* displayName,
        const char* extension,
        YiCadImportCallback callback,
        void* userData = nullptr) const noexcept
    {
        return pluginId != nullptr && formatId != nullptr &&
               displayName != nullptr && extension != nullptr &&
               callback != nullptr &&
               hasField(
                   offsetof(YiCadHostApi, registerImportFilter),
                   sizeof(m_api->registerImportFilter)) &&
               m_api->registerImportFilter != nullptr &&
               m_api->registerImportFilter(
                   pluginId,
                   formatId,
                   displayName,
                   extension,
                   callback,
                   userData) == YICAD_SUCCESS;
    }

    bool registerExportFilter(
        const char* pluginId,
        const char* formatId,
        const char* displayName,
        const char* extension,
        YiCadExportCallback callback,
        void* userData = nullptr) const noexcept
    {
        return pluginId != nullptr && formatId != nullptr &&
               displayName != nullptr && extension != nullptr &&
               callback != nullptr &&
               hasField(
                   offsetof(YiCadHostApi, registerExportFilter),
                   sizeof(m_api->registerExportFilter)) &&
               m_api->registerExportFilter != nullptr &&
               m_api->registerExportFilter(
                   pluginId,
                   formatId,
                   displayName,
                   extension,
                   callback,
                   userData) == YICAD_SUCCESS;
    }

    Document currentDocument() const noexcept
    {
        if (!hasField(
                offsetof(YiCadHostApi, currentDocument),
                sizeof(m_api->currentDocument)) ||
            m_api->currentDocument == nullptr)
        {
            return {};
        }

        return Document(m_api, m_api->currentDocument());
    }

    /// @brief 将文件回调收到的文档句柄包装为 SDK 文档对象。
    Document document(YiCadDocumentHandle handle) const noexcept
    {
        return isCompatible() && handle != nullptr
            ? Document(m_api, handle)
            : Document();
    }

private:
    bool isCompatible() const noexcept
    {
        return m_api != nullptr &&
               m_api->structSize >=
                   offsetof(YiCadHostApi, abiVersion) +
                       sizeof(m_api->abiVersion) &&
               m_api->abiVersion >= YICAD_PLUGIN_ABI_MIN_VERSION &&
               m_api->abiVersion <= YICAD_PLUGIN_ABI_MAX_VERSION;
    }

    bool hasField(size_t offset, size_t size) const noexcept
    {
        return isCompatible() && m_api->structSize >= offset + size;
    }

    const YiCadHostApi* m_api = nullptr;
};

} // namespace yicad::plugin

#endif
