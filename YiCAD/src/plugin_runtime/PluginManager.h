#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "NativePluginLoader.h"
#include "PluginManifestReader.h"
#include "PluginRegistry.h"

#include <QString>
#include <QVector>

#include <cstdint>
#include <memory>
#include <vector>

class HostApi;

enum class PluginLifecycleState
{
    Discovered,
    Validated,
    Loaded,
    Initialized,
    Active,
    Shutdown
};

enum class PluginManagerErrorCode
{
    None,
    InvalidManifest,
    HostApiUnavailable,
    HostApiLayoutMismatch,
    LoadFailed,
    AbiVersionQueryFailed,
    AbiVersionMismatch,
    RegistrationBeginFailed,
    InitFailed,
    InitThrewException,
    PluginApiLayoutMismatch,
    PluginApiVersionMismatch,
    InvalidPluginId,
    InvalidPluginName,
    InvalidPluginVersion,
    RegistryCommitFailed,
    UnexpectedFailure,
    ShutdownFailed
};

struct PluginManagerError
{
    PluginManagerErrorCode code = PluginManagerErrorCode::None;
    QString message;
    PluginManifestErrorCode manifestCode = PluginManifestErrorCode::None;
    NativePluginLoadErrorCode loadCode = NativePluginLoadErrorCode::None;
    PluginRegistryErrorCode registryCode = PluginRegistryErrorCode::None;

    bool isError() const noexcept;
};

/// @brief 保存单个插件清单在本次加载流程中达到的状态和诊断信息。
struct PluginManagerRecord
{
    QString manifestPath;
    QString dllPath;
    QString dllDirectory;
    QString pluginId;
    QString pluginName;
    QString pluginVersion;
    std::uint32_t pluginAbiVersion = 0;
    std::uint32_t negotiatedAbiVersion = 0;
    PluginLifecycleState state = PluginLifecycleState::Discovered;
    PluginManagerError error;
    bool initInvoked = false;
    bool shutdownCalled = false;
    bool shutdownSucceeded = false;
};

/// @brief 串联插件发现、原生加载、注册事务和生命周期关闭。
/// @note 第一版只允许加载一次，不支持热加载或运行中卸载。
class PluginManager
{
public:
    PluginManager(HostApi& hostApi, PluginRegistry& registry);
    PluginManager(
        HostApi& hostApi,
        PluginRegistry& registry,
        QString discoveryDirectory);
    ~PluginManager() noexcept;

    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;
    PluginManager(PluginManager&&) = delete;
    PluginManager& operator=(PluginManager&&) = delete;

    /// @brief 按清单确定性顺序加载全部插件；重复调用不会再次加载。
    void loadAll();

    /// @brief 按成功初始化的逆序关闭插件；重复调用安全。
    void shutdownAll() noexcept;

    const QVector<PluginManagerRecord>& records() const noexcept;
    bool hasLoaded() const noexcept;
    bool isShutdown() const noexcept;

    /// @brief 判断插件是否仍处于 Active 状态。
    bool isPluginActive(const QString& pluginId) const noexcept;

private:
    struct ActivePlugin;

    void loadManifest(
        const PluginManifestReadResult& manifestResult,
        std::size_t recordIndex);
    bool invokeShutdown(
        ActivePlugin& plugin,
        PluginManagerRecord& record) noexcept;
    void cleanupFailedPlugin(
        ActivePlugin& plugin,
        PluginManagerRecord& record) noexcept;

    HostApi& m_hostApi;
    PluginRegistry& m_registry;
    PluginManifestReader m_manifestReader;
    QVector<PluginManagerRecord> m_records;
    std::vector<std::unique_ptr<ActivePlugin>> m_activePlugins;
    bool m_loadAttempted = false;
    bool m_shutdown = false;
};

#endif // PLUGIN_MANAGER_H
