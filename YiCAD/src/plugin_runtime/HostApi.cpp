#include "HostApi.h"

#include "DmCircle.h"
#include "DmDocument.h"
#include "DmLine.h"
#include "GuiDocumentView.h"
#include "PluginRegistry.h"
#include "Transaction.h"

#include <QString>

#include <cmath>
#include <memory>

namespace
{

YiCadResult toResult(bool success) noexcept
{
    return success ? YICAD_SUCCESS : YICAD_FAILURE;
}

bool allFinite(double first, double second, double third, double fourth) noexcept
{
    return std::isfinite(first) && std::isfinite(second) &&
           std::isfinite(third) && std::isfinite(fourth);
}

bool copyUtf8(const char* source, QString& target)
{
    if (source == nullptr)
    {
        return false;
    }
    target = QString::fromUtf8(source);
    return true;
}

} // namespace

struct HostApi::DocumentHandleRecord
{
    DmDocument* document = nullptr;
};

thread_local HostApi* HostApi::s_activeInstance = nullptr;

HostApi::HostApi(
    PluginHostContext& context,
    PluginRegistry& registry) noexcept
    : m_context(context)
    , m_registry(registry)
    , m_api{
          static_cast<uint32_t>(sizeof(YiCadHostApi)),
          YICAD_PLUGIN_ABI_MAX_VERSION,
          &HostApi::message,
          &HostApi::registerCommand,
          &HostApi::registerRibbonButton,
          &HostApi::currentDocument,
          &HostApi::documentAddLine,
          &HostApi::documentAddCircle,
          &HostApi::documentRegen,
          &HostApi::documentZoomAuto,
          &HostApi::registerImportFilter,
          &HostApi::registerExportFilter}
{
    if (s_activeInstance == nullptr)
    {
        s_activeInstance = this;
        m_active = true;
    }
}

HostApi::~HostApi()
{
    if (s_activeInstance == this)
    {
        s_activeInstance = nullptr;
    }
}

const YiCadHostApi* HostApi::api() const noexcept
{
    return m_active ? &m_api : nullptr;
}

bool HostApi::isActive() const noexcept
{
    return m_active;
}

YiCadDocumentHandle HostApi::documentHandle(DmDocument* document) noexcept
{
    try
    {
        if (!m_active || document == nullptr ||
            !m_context.isDocumentOpen(document))
        {
            return nullptr;
        }
        return handleForDocument(document);
    }
    catch (...)
    {
        return nullptr;
    }
}

bool HostApi::isDocumentHandleValid(
    YiCadDocumentHandle handle) const noexcept
{
    return m_active && resolveDocument(handle) != nullptr;
}

HostApi* HostApi::activeInstance() noexcept
{
    return s_activeInstance;
}

void YICAD_PLUGIN_CALL HostApi::message(const char* text) noexcept
{
    try
    {
        auto* instance = activeInstance();
        QString messageText;
        if (instance != nullptr && copyUtf8(text, messageText))
        {
            instance->m_context.showPluginMessage(messageText);
        }
    }
    catch (...)
    {
    }
}

YiCadResult YICAD_PLUGIN_CALL HostApi::registerCommand(
    const char* pluginId,
    const char* commandId,
    const char* displayName,
    YiCadCommandCallback callback,
    void* userData) noexcept
{
    try
    {
        auto* instance = activeInstance();
        QString plugin;
        QString command;
        QString name;
        if (instance == nullptr || !copyUtf8(pluginId, plugin) ||
            !copyUtf8(commandId, command) ||
            !copyUtf8(displayName, name))
        {
            return YICAD_FAILURE;
        }
        return toResult(instance->m_registry.stageCommand(
            plugin, command, name, callback, userData));
    }
    catch (...)
    {
        return YICAD_FAILURE;
    }
}

YiCadResult YICAD_PLUGIN_CALL HostApi::registerRibbonButton(
    const char* pluginId,
    const char* tab,
    const char* group,
    const char* commandId,
    const char* iconPath) noexcept
{
    try
    {
        auto* instance = activeInstance();
        QString plugin;
        QString tabName;
        QString groupName;
        QString command;
        QString icon;
        if (instance == nullptr || !copyUtf8(pluginId, plugin) ||
            !copyUtf8(tab, tabName) || !copyUtf8(group, groupName) ||
            !copyUtf8(commandId, command) || !copyUtf8(iconPath, icon))
        {
            return YICAD_FAILURE;
        }
        return toResult(instance->m_registry.stageRibbonButton(
            plugin, tabName, groupName, command, icon));
    }
    catch (...)
    {
        return YICAD_FAILURE;
    }
}

YiCadDocumentHandle YICAD_PLUGIN_CALL HostApi::currentDocument() noexcept
{
    try
    {
        auto* instance = activeInstance();
        if (instance == nullptr)
        {
            return nullptr;
        }

        auto* document = instance->m_context.currentDocument();
        auto* view = document == nullptr
            ? nullptr
            : instance->m_context.documentView(document);
        if (document == nullptr ||
            !instance->m_context.isDocumentOpen(document) ||
            view == nullptr || view->getDocument() != document)
        {
            return nullptr;
        }
        return instance->handleForDocument(document);
    }
    catch (...)
    {
        return nullptr;
    }
}

YiCadResult YICAD_PLUGIN_CALL HostApi::documentAddLine(
    YiCadDocumentHandle documentHandle,
    double x1,
    double y1,
    double x2,
    double y2) noexcept
{
    try
    {
        auto* instance = activeInstance();
        if (instance == nullptr || !allFinite(x1, y1, x2, y2) ||
            (x1 == x2 && y1 == y2))
        {
            return YICAD_FAILURE;
        }

        auto* document = instance->resolveDocument(documentHandle);
        auto* table = document == nullptr ? nullptr : document->getEntityTable();
        if (table == nullptr)
        {
            return YICAD_FAILURE;
        }

        auto entity = std::make_unique<DmLine>(
            nullptr, DmVector(x1, y1), DmVector(x2, y2));
        entity->setDocument(document);
        entity->update();

        Transaction transaction("Plugin: Add Line", document);
        transaction.start();
        table->add(entity.get());
        entity.release();
        transaction.commit();
        return YICAD_SUCCESS;
    }
    catch (...)
    {
        return YICAD_FAILURE;
    }
}

YiCadResult YICAD_PLUGIN_CALL HostApi::documentAddCircle(
    YiCadDocumentHandle documentHandle,
    double centerX,
    double centerY,
    double radius) noexcept
{
    try
    {
        auto* instance = activeInstance();
        if (instance == nullptr || !std::isfinite(centerX) ||
            !std::isfinite(centerY) || !std::isfinite(radius) ||
            radius <= 0.0)
        {
            return YICAD_FAILURE;
        }

        auto* document = instance->resolveDocument(documentHandle);
        auto* table = document == nullptr ? nullptr : document->getEntityTable();
        if (table == nullptr)
        {
            return YICAD_FAILURE;
        }

        auto entity = std::make_unique<DmCircle>(
            nullptr, CircleData(DmVector(centerX, centerY), radius));
        entity->setDocument(document);
        entity->update();

        Transaction transaction("Plugin: Add Circle", document);
        transaction.start();
        table->add(entity.get());
        entity.release();
        transaction.commit();
        return YICAD_SUCCESS;
    }
    catch (...)
    {
        return YICAD_FAILURE;
    }
}

YiCadResult YICAD_PLUGIN_CALL HostApi::documentRegen(
    YiCadDocumentHandle documentHandle) noexcept
{
    try
    {
        auto* instance = activeInstance();
        GuiDocumentView* view = nullptr;
        auto* document = instance == nullptr
            ? nullptr
            : instance->resolveDocument(documentHandle, &view);
        if (document == nullptr || view == nullptr)
        {
            return YICAD_FAILURE;
        }

        document->setDocumentView(view);
        document->regenerate();
        return YICAD_SUCCESS;
    }
    catch (...)
    {
        return YICAD_FAILURE;
    }
}

YiCadResult YICAD_PLUGIN_CALL HostApi::documentZoomAuto(
    YiCadDocumentHandle documentHandle) noexcept
{
    try
    {
        auto* instance = activeInstance();
        GuiDocumentView* view = nullptr;
        auto* document = instance == nullptr
            ? nullptr
            : instance->resolveDocument(documentHandle, &view);
        if (document == nullptr || view == nullptr)
        {
            return YICAD_FAILURE;
        }

        view->zoomAuto();
        return YICAD_SUCCESS;
    }
    catch (...)
    {
        return YICAD_FAILURE;
    }
}

YiCadResult YICAD_PLUGIN_CALL HostApi::registerImportFilter(
    const char* pluginId,
    const char* formatId,
    const char* displayName,
    const char* extension,
    YiCadImportCallback callback,
    void* userData) noexcept
{
    try
    {
        auto* instance = activeInstance();
        QString plugin;
        QString format;
        QString name;
        QString suffix;
        if (instance == nullptr || !copyUtf8(pluginId, plugin) ||
            !copyUtf8(formatId, format) || !copyUtf8(displayName, name) ||
            !copyUtf8(extension, suffix))
        {
            return YICAD_FAILURE;
        }
        return toResult(instance->m_registry.stageImportFilter(
            plugin, format, name, suffix, callback, userData));
    }
    catch (...)
    {
        return YICAD_FAILURE;
    }
}

YiCadResult YICAD_PLUGIN_CALL HostApi::registerExportFilter(
    const char* pluginId,
    const char* formatId,
    const char* displayName,
    const char* extension,
    YiCadExportCallback callback,
    void* userData) noexcept
{
    try
    {
        auto* instance = activeInstance();
        QString plugin;
        QString format;
        QString name;
        QString suffix;
        if (instance == nullptr || !copyUtf8(pluginId, plugin) ||
            !copyUtf8(formatId, format) || !copyUtf8(displayName, name) ||
            !copyUtf8(extension, suffix))
        {
            return YICAD_FAILURE;
        }
        return toResult(instance->m_registry.stageExportFilter(
            plugin, format, name, suffix, callback, userData));
    }
    catch (...)
    {
        return YICAD_FAILURE;
    }
}

YiCadDocumentHandle HostApi::handleForDocument(DmDocument* document)
{
    for (const auto& record : m_documentHandles)
    {
        if (record->document == document)
        {
            return static_cast<void*>(record.get());
        }
    }

    auto record = std::make_unique<DocumentHandleRecord>();
    record->document = document;
    auto* handle = record.get();
    m_documentHandles.push_back(std::move(record));
    return static_cast<void*>(handle);
}

DmDocument* HostApi::resolveDocument(
    YiCadDocumentHandle handle,
    GuiDocumentView** view) const noexcept
{
    if (view != nullptr)
    {
        *view = nullptr;
    }
    if (handle == nullptr)
    {
        return nullptr;
    }

    for (const auto& record : m_documentHandles)
    {
        if (static_cast<const void*>(record.get()) != handle)
        {
            continue;
        }

        auto* document = record->document;
        if (document == nullptr || !m_context.isDocumentOpen(document))
        {
            return nullptr;
        }
        if (view == nullptr)
        {
            return document;
        }

        auto* documentView = m_context.documentView(document);
        if (documentView == nullptr || documentView->getDocument() != document)
        {
            return nullptr;
        }
        *view = documentView;
        return document;
    }
    return nullptr;
}
