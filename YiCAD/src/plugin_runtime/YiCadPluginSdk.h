#ifndef YICAD_PLUGIN_SDK_H
#define YICAD_PLUGIN_SDK_H

#include "YiCadPluginAbi.h"

namespace yicad::plugin
{

class Host;
class Document;

/// @brief 宿主持有的不透明文档事务；析构时自动回滚未提交事务。
class DocumentTransaction
{
public:
    DocumentTransaction() noexcept = default;
    DocumentTransaction(const DocumentTransaction&) = delete;
    DocumentTransaction& operator=(const DocumentTransaction&) = delete;

    DocumentTransaction(DocumentTransaction&& other) noexcept
        : m_api(other.m_api),
          m_handle(other.m_handle)
    {
        other.m_api = nullptr;
        other.m_handle = nullptr;
    }

    DocumentTransaction& operator=(DocumentTransaction&& other) noexcept
    {
        if (this != &other)
        {
            if (!rollback())
            {
                return *this;
            }
            m_api = other.m_api;
            m_handle = other.m_handle;
            other.m_api = nullptr;
            other.m_handle = nullptr;
        }
        return *this;
    }

    ~DocumentTransaction()
    {
        rollback();
    }

    explicit operator bool() const noexcept
    {
        return m_api != nullptr && m_handle != nullptr;
    }

    bool commit() noexcept
    {
        if (!m_api || !m_handle || !m_api->documentCommitTransaction ||
            m_api->documentCommitTransaction(m_handle) != YICAD_SUCCESS)
        {
            return false;
        }
        m_handle = nullptr;
        return true;
    }

    bool rollback() noexcept
    {
        if (m_handle == nullptr)
        {
            return true;
        }
        if (m_api == nullptr || !m_api->documentRollbackTransaction ||
            m_api->documentRollbackTransaction(m_handle) != YICAD_SUCCESS)
        {
            return false;
        }
        m_handle = nullptr;
        return true;
    }

private:
    friend class Document;

    DocumentTransaction(
        const YiCadHostApi* api,
        YiCadTransactionHandle handle) noexcept
        : m_api(api),
          m_handle(handle)
    {
    }

    const YiCadHostApi* m_api = nullptr;
    YiCadTransactionHandle m_handle = nullptr;
};

/// @brief 只读实体快照迭代器；析构时将不透明句柄归还宿主。
class EntityIterator
{
public:
    EntityIterator() noexcept = default;
    EntityIterator(const EntityIterator&) = delete;
    EntityIterator& operator=(const EntityIterator&) = delete;

    EntityIterator(EntityIterator&& other) noexcept
        : m_api(other.m_api),
          m_handle(other.m_handle)
    {
        other.m_api = nullptr;
        other.m_handle = nullptr;
    }

    EntityIterator& operator=(EntityIterator&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            m_api = other.m_api;
            m_handle = other.m_handle;
            other.m_api = nullptr;
            other.m_handle = nullptr;
        }
        return *this;
    }

    ~EntityIterator()
    {
        reset();
    }

    explicit operator bool() const noexcept
    {
        return m_api != nullptr && m_handle != nullptr;
    }

    bool next(YiCadEntityType& type) noexcept
    {
        return m_api && m_handle && m_api->entityIteratorNext &&
               m_api->entityIteratorNext(m_handle, &type) == YICAD_SUCCESS;
    }

    bool line(YiCadLineData& data) const noexcept
    {
        return m_api && m_handle && m_api->entityIteratorGetLine &&
               m_api->entityIteratorGetLine(m_handle, &data) ==
                   YICAD_SUCCESS;
    }

    bool circle(YiCadCircleData& data) const noexcept
    {
        return m_api && m_handle && m_api->entityIteratorGetCircle &&
               m_api->entityIteratorGetCircle(m_handle, &data) ==
                   YICAD_SUCCESS;
    }

private:
    friend class Document;

    EntityIterator(
        const YiCadHostApi* api,
        YiCadEntityIteratorHandle handle) noexcept
        : m_api(api),
          m_handle(handle)
    {
    }

    void reset() noexcept
    {
        if (m_api && m_handle && m_api->entityIteratorDestroy)
        {
            m_api->entityIteratorDestroy(m_handle);
        }
        m_handle = nullptr;
    }

    const YiCadHostApi* m_api = nullptr;
    YiCadEntityIteratorHandle m_handle = nullptr;
};

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

    /// @brief 开始一个整体可撤销的 ABI v2 文档事务。
    DocumentTransaction beginTransaction(const char* name) const noexcept
    {
        if (name == nullptr || *name == '\0' ||
            !hasV2Field(
                offsetof(YiCadHostApi, documentRollbackTransaction),
                sizeof(m_api->documentRollbackTransaction)) ||
            m_api->documentBeginTransaction == nullptr)
        {
            return {};
        }
        return DocumentTransaction(
            m_api, m_api->documentBeginTransaction(m_handle, name));
    }

    /// @brief 创建与文档后续修改无关的只读实体数据快照。
    EntityIterator entities() const noexcept
    {
        if (!hasV2Field(
                offsetof(YiCadHostApi, entityIteratorDestroy),
                sizeof(m_api->entityIteratorDestroy)) ||
            m_api->documentCreateEntityIterator == nullptr)
        {
            return {};
        }
        return EntityIterator(
            m_api, m_api->documentCreateEntityIterator(m_handle));
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

    bool hasV2Field(size_t offset, size_t size) const noexcept
    {
        return hasField(offset, size) &&
               m_api->abiVersion >= YICAD_PLUGIN_ABI_V2;
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
