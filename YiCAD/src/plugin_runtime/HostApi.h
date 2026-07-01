#ifndef HOST_API_H
#define HOST_API_H

#include "YiCadPluginAbi.h"

#include <memory>
#include <vector>

class DmDocument;
class GuiDocumentView;
class PluginRegistry;
class QString;

/// @brief HostApi 访问应用状态所需的最小宿主上下文。
/// @note 上下文及其返回的对象由应用持有，并且必须比 HostApi 生存更久。
class PluginHostContext
{
public:
    virtual ~PluginHostContext() = default;

    /// @brief 向用户显示一条插件消息。
    virtual void showPluginMessage(const QString& message) = 0;

    /// @brief 返回当前文档；没有活动文档时返回 nullptr。
    virtual DmDocument* currentDocument() const noexcept = 0;

    /// @brief 判断文档地址是否仍属于一个已打开文档。
    /// @note 实现只能比较地址或查询宿主容器，不得解引用未知或已关闭的文档。
    virtual bool isDocumentOpen(const DmDocument* document) const noexcept = 0;

    /// @brief 返回文档当前有效的视图；不存在时返回 nullptr。
    virtual GuiDocumentView* documentView(
        const DmDocument* document) const noexcept = 0;
};

/// @brief 将 YiCAD 内部注册与文档能力适配为稳定的 C ABI 函数表。
/// @note ABI 回调只允许从创建 HostApi 的 UI 线程调用。
class HostApi
{
public:
    HostApi(PluginHostContext& context, PluginRegistry& registry) noexcept;
    ~HostApi();

    HostApi(const HostApi&) = delete;
    HostApi& operator=(const HostApi&) = delete;
    HostApi(HostApi&&) = delete;
    HostApi& operator=(HostApi&&) = delete;

    /// @brief 返回生命周期稳定的宿主函数表。
    /// @return 当前线程已有其他 HostApi 时返回 nullptr。
    const YiCadHostApi* api() const noexcept;

    /// @brief 判断当前对象是否拥有本线程的 ABI 回调入口。
    bool isActive() const noexcept;

    /// @brief 为仍处于打开状态的文档取得非拥有型 ABI 句柄。
    YiCadDocumentHandle documentHandle(DmDocument* document) noexcept;

    /// @brief 验证 ABI 文档句柄当前仍可解析到打开的文档。
    bool isDocumentHandleValid(YiCadDocumentHandle handle) const noexcept;

private:
    struct DocumentHandleRecord;
    struct TransactionRecord;
    struct EntityIteratorRecord;

    static HostApi* activeInstance() noexcept;

    static void YICAD_PLUGIN_CALL message(const char* text) noexcept;
    static YiCadResult YICAD_PLUGIN_CALL registerCommand(
        const char* pluginId,
        const char* commandId,
        const char* displayName,
        YiCadCommandCallback callback,
        void* userData) noexcept;
    static YiCadResult YICAD_PLUGIN_CALL registerRibbonButton(
        const char* pluginId,
        const char* tab,
        const char* group,
        const char* commandId,
        const char* iconPath) noexcept;
    static YiCadDocumentHandle YICAD_PLUGIN_CALL currentDocument() noexcept;
    static YiCadResult YICAD_PLUGIN_CALL documentAddLine(
        YiCadDocumentHandle document,
        double x1,
        double y1,
        double x2,
        double y2) noexcept;
    static YiCadResult YICAD_PLUGIN_CALL documentAddCircle(
        YiCadDocumentHandle document,
        double centerX,
        double centerY,
        double radius) noexcept;
    static YiCadResult YICAD_PLUGIN_CALL documentRegen(
        YiCadDocumentHandle document) noexcept;
    static YiCadResult YICAD_PLUGIN_CALL documentZoomAuto(
        YiCadDocumentHandle document) noexcept;
    static YiCadResult YICAD_PLUGIN_CALL registerImportFilter(
        const char* pluginId,
        const char* formatId,
        const char* displayName,
        const char* extension,
        YiCadImportCallback callback,
        void* userData) noexcept;
    static YiCadResult YICAD_PLUGIN_CALL registerExportFilter(
        const char* pluginId,
        const char* formatId,
        const char* displayName,
        const char* extension,
        YiCadExportCallback callback,
        void* userData) noexcept;
    static YiCadTransactionHandle YICAD_PLUGIN_CALL documentBeginTransaction(
        YiCadDocumentHandle document,
        const char* name) noexcept;
    static YiCadResult YICAD_PLUGIN_CALL documentCommitTransaction(
        YiCadTransactionHandle transaction) noexcept;
    static YiCadResult YICAD_PLUGIN_CALL documentRollbackTransaction(
        YiCadTransactionHandle transaction) noexcept;
    static YiCadEntityIteratorHandle YICAD_PLUGIN_CALL
    documentCreateEntityIterator(YiCadDocumentHandle document) noexcept;
    static YiCadResult YICAD_PLUGIN_CALL entityIteratorNext(
        YiCadEntityIteratorHandle iterator,
        YiCadEntityType* entityType) noexcept;
    static YiCadResult YICAD_PLUGIN_CALL entityIteratorGetLine(
        YiCadEntityIteratorHandle iterator,
        YiCadLineData* line) noexcept;
    static YiCadResult YICAD_PLUGIN_CALL entityIteratorGetCircle(
        YiCadEntityIteratorHandle iterator,
        YiCadCircleData* circle) noexcept;
    static void YICAD_PLUGIN_CALL entityIteratorDestroy(
        YiCadEntityIteratorHandle iterator) noexcept;

    YiCadDocumentHandle handleForDocument(DmDocument* document);
    DmDocument* resolveDocument(
        YiCadDocumentHandle handle,
        GuiDocumentView** view = nullptr) const noexcept;
    TransactionRecord* resolveTransaction(
        YiCadTransactionHandle handle) const noexcept;
    EntityIteratorRecord* resolveEntityIterator(
        YiCadEntityIteratorHandle handle) const noexcept;
    bool hasActiveTransaction(const DmDocument* document) const noexcept;

    PluginHostContext& m_context;
    PluginRegistry& m_registry;
    YiCadHostApi m_api;
    std::vector<std::unique_ptr<DocumentHandleRecord>> m_documentHandles;
    std::vector<std::unique_ptr<TransactionRecord>> m_transactions;
    std::vector<std::unique_ptr<EntityIteratorRecord>> m_entityIterators;
    bool m_active = false;

    static thread_local HostApi* s_activeInstance;
};

#endif // HOST_API_H
