#include "HostApi.h"

#include "DmCircle.h"
#include "DmDocument.h"
#include "DmLine.h"
#include "EntityTable.h"
#include "GuiDocumentView.h"
#include "PluginRegistry.h"
#include "Transaction.h"

#include <QString>

#include <cmath>
#include <algorithm>
#include <memory>
#include <string>

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

struct HostApi::TransactionRecord
{
    DmDocument* document = nullptr;
    std::unique_ptr<Transaction> transaction;
};

struct HostApi::EntityIteratorRecord
{
    struct EntitySnapshot
    {
        YiCadEntityType type = YICAD_ENTITY_UNKNOWN;
        YiCadLineData line{};
        YiCadCircleData circle{};
    };

    std::vector<EntitySnapshot> entities;
    std::size_t nextIndex = 0;
    std::size_t currentIndex = 0;
    bool hasCurrent = false;
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
          &HostApi::registerExportFilter,
          &HostApi::documentBeginTransaction,
          &HostApi::documentCommitTransaction,
          &HostApi::documentRollbackTransaction,
          &HostApi::documentCreateEntityIterator,
          &HostApi::entityIteratorNext,
          &HostApi::entityIteratorGetLine,
          &HostApi::entityIteratorGetCircle,
          &HostApi::entityIteratorDestroy}
{
    if (s_activeInstance == nullptr)
    {
        s_activeInstance = this;
        m_active = true;
    }
}

HostApi::~HostApi()
{
    for (auto& record : m_transactions)
    {
        try
        {
            if (m_context.isDocumentOpen(record->document))
            {
                record->transaction->rollback();
            }
        }
        catch (...)
        {
        }
    }
    m_transactions.clear();
    m_entityIterators.clear();

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

        if (instance->hasActiveTransaction(document))
        {
            table->add(entity.get());
            entity.release();
        }
        else
        {
            Transaction transaction("Plugin: Add Line", document);
            transaction.start();
            try
            {
                table->add(entity.get());
                entity.release();
                transaction.commit();
            }
            catch (...)
            {
                transaction.rollback();
                throw;
            }
        }
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

        if (instance->hasActiveTransaction(document))
        {
            table->add(entity.get());
            entity.release();
        }
        else
        {
            Transaction transaction("Plugin: Add Circle", document);
            transaction.start();
            try
            {
                table->add(entity.get());
                entity.release();
                transaction.commit();
            }
            catch (...)
            {
                transaction.rollback();
                throw;
            }
        }
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

YiCadTransactionHandle YICAD_PLUGIN_CALL HostApi::documentBeginTransaction(
    YiCadDocumentHandle documentHandle,
    const char* name) noexcept
{
    try
    {
        auto* instance = activeInstance();
        QString transactionName;
        auto* document = instance == nullptr
            ? nullptr
            : instance->resolveDocument(documentHandle);
        if (document == nullptr || !copyUtf8(name, transactionName) ||
            transactionName.trimmed().isEmpty() ||
            instance->hasActiveTransaction(document) ||
            document->getCmdManager() == nullptr ||
            document->getCmdManager()->getCurrentCmd() != nullptr ||
            document->getCmdManager()->getCurrentGroupCmd() != nullptr)
        {
            return nullptr;
        }

        auto record = std::make_unique<TransactionRecord>();
        record->document = document;
        record->transaction = std::make_unique<Transaction>(
            transactionName.toUtf8().constData(), document);
        record->transaction->start();
        auto* handle = record.get();
        instance->m_transactions.push_back(std::move(record));
        return static_cast<void*>(handle);
    }
    catch (...)
    {
        return nullptr;
    }
}

YiCadResult YICAD_PLUGIN_CALL HostApi::documentCommitTransaction(
    YiCadTransactionHandle transactionHandle) noexcept
{
    try
    {
        auto* instance = activeInstance();
        auto* record = instance == nullptr
            ? nullptr
            : instance->resolveTransaction(transactionHandle);
        if (record == nullptr ||
            !instance->m_context.isDocumentOpen(record->document))
        {
            return YICAD_FAILURE;
        }

        record->transaction->commit();
        instance->m_transactions.erase(std::remove_if(
            instance->m_transactions.begin(),
            instance->m_transactions.end(),
            [record](const auto& item) { return item.get() == record; }));
        return YICAD_SUCCESS;
    }
    catch (...)
    {
        return YICAD_FAILURE;
    }
}

YiCadResult YICAD_PLUGIN_CALL HostApi::documentRollbackTransaction(
    YiCadTransactionHandle transactionHandle) noexcept
{
    try
    {
        auto* instance = activeInstance();
        auto* record = instance == nullptr
            ? nullptr
            : instance->resolveTransaction(transactionHandle);
        if (record == nullptr ||
            !instance->m_context.isDocumentOpen(record->document))
        {
            return YICAD_FAILURE;
        }

        record->transaction->rollback();
        instance->m_transactions.erase(std::remove_if(
            instance->m_transactions.begin(),
            instance->m_transactions.end(),
            [record](const auto& item) { return item.get() == record; }));
        return YICAD_SUCCESS;
    }
    catch (...)
    {
        return YICAD_FAILURE;
    }
}

YiCadEntityIteratorHandle YICAD_PLUGIN_CALL
HostApi::documentCreateEntityIterator(
    YiCadDocumentHandle documentHandle) noexcept
{
    try
    {
        auto* instance = activeInstance();
        auto* document = instance == nullptr
            ? nullptr
            : instance->resolveDocument(documentHandle);
        auto* table = document == nullptr ? nullptr : document->getEntityTable();
        if (table == nullptr)
        {
            return nullptr;
        }

        auto record = std::make_unique<EntityIteratorRecord>();
        record->entities.reserve(static_cast<std::size_t>(table->count()));
        for (auto* entity : *table)
        {
            EntityIteratorRecord::EntitySnapshot snapshot;
            if (auto* line = dynamic_cast<DmLine*>(entity))
            {
                const auto start = line->getStartpoint();
                const auto end = line->getEndpoint();
                snapshot.type = YICAD_ENTITY_LINE;
                snapshot.line = {start.x, start.y, end.x, end.y};
            }
            else if (auto* circle = dynamic_cast<DmCircle*>(entity))
            {
                const auto center = circle->getCenter();
                snapshot.type = YICAD_ENTITY_CIRCLE;
                snapshot.circle = {
                    center.x, center.y, circle->getRadius()};
            }
            record->entities.push_back(snapshot);
        }

        auto* handle = record.get();
        instance->m_entityIterators.push_back(std::move(record));
        return static_cast<void*>(handle);
    }
    catch (...)
    {
        return nullptr;
    }
}

YiCadResult YICAD_PLUGIN_CALL HostApi::entityIteratorNext(
    YiCadEntityIteratorHandle iteratorHandle,
    YiCadEntityType* entityType) noexcept
{
    auto* instance = activeInstance();
    auto* iterator = instance == nullptr
        ? nullptr
        : instance->resolveEntityIterator(iteratorHandle);
    if (iterator == nullptr || entityType == nullptr ||
        iterator->nextIndex >= iterator->entities.size())
    {
        if (iterator != nullptr)
        {
            iterator->hasCurrent = false;
        }
        return YICAD_FAILURE;
    }

    iterator->currentIndex = iterator->nextIndex++;
    iterator->hasCurrent = true;
    *entityType = iterator->entities[iterator->currentIndex].type;
    return YICAD_SUCCESS;
}

YiCadResult YICAD_PLUGIN_CALL HostApi::entityIteratorGetLine(
    YiCadEntityIteratorHandle iteratorHandle,
    YiCadLineData* line) noexcept
{
    auto* instance = activeInstance();
    auto* iterator = instance == nullptr
        ? nullptr
        : instance->resolveEntityIterator(iteratorHandle);
    if (iterator == nullptr || line == nullptr || !iterator->hasCurrent)
    {
        return YICAD_FAILURE;
    }
    const auto& current = iterator->entities[iterator->currentIndex];
    if (current.type != YICAD_ENTITY_LINE)
    {
        return YICAD_FAILURE;
    }
    *line = current.line;
    return YICAD_SUCCESS;
}

YiCadResult YICAD_PLUGIN_CALL HostApi::entityIteratorGetCircle(
    YiCadEntityIteratorHandle iteratorHandle,
    YiCadCircleData* circle) noexcept
{
    auto* instance = activeInstance();
    auto* iterator = instance == nullptr
        ? nullptr
        : instance->resolveEntityIterator(iteratorHandle);
    if (iterator == nullptr || circle == nullptr || !iterator->hasCurrent)
    {
        return YICAD_FAILURE;
    }
    const auto& current = iterator->entities[iterator->currentIndex];
    if (current.type != YICAD_ENTITY_CIRCLE)
    {
        return YICAD_FAILURE;
    }
    *circle = current.circle;
    return YICAD_SUCCESS;
}

void YICAD_PLUGIN_CALL HostApi::entityIteratorDestroy(
    YiCadEntityIteratorHandle iteratorHandle) noexcept
{
    try
    {
        auto* instance = activeInstance();
        auto* iterator = instance == nullptr
            ? nullptr
            : instance->resolveEntityIterator(iteratorHandle);
        if (iterator == nullptr)
        {
            return;
        }
        instance->m_entityIterators.erase(std::remove_if(
            instance->m_entityIterators.begin(),
            instance->m_entityIterators.end(),
            [iterator](const auto& item) { return item.get() == iterator; }));
    }
    catch (...)
    {
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

HostApi::TransactionRecord* HostApi::resolveTransaction(
    YiCadTransactionHandle handle) const noexcept
{
    if (handle == nullptr)
    {
        return nullptr;
    }
    for (const auto& record : m_transactions)
    {
        if (static_cast<const void*>(record.get()) == handle)
        {
            return record.get();
        }
    }
    return nullptr;
}

HostApi::EntityIteratorRecord* HostApi::resolveEntityIterator(
    YiCadEntityIteratorHandle handle) const noexcept
{
    if (handle == nullptr)
    {
        return nullptr;
    }
    for (const auto& record : m_entityIterators)
    {
        if (static_cast<const void*>(record.get()) == handle)
        {
            return record.get();
        }
    }
    return nullptr;
}

bool HostApi::hasActiveTransaction(
    const DmDocument* document) const noexcept
{
    for (const auto& record : m_transactions)
    {
        if (record->document == document)
        {
            return true;
        }
    }
    return false;
}
