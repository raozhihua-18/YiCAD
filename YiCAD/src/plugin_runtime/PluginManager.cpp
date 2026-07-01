#include "PluginManager.h"

#include "HostApi.h"

#include <cstddef>
#include <utility>

namespace
{

constexpr std::uint32_t PluginApiCapacity =
    static_cast<std::uint32_t>(sizeof(YiCadPluginApi));
constexpr std::uint32_t PluginApiV1RequiredSize =
    YICAD_PLUGIN_API_V1_SIZE;
constexpr std::uint32_t AbiHeaderSize =
    static_cast<std::uint32_t>(
        offsetof(YiCadHostApi, abiVersion) +
        sizeof(std::uint32_t));

void setError(
    PluginManagerRecord& record,
    PluginManagerErrorCode code,
    QString message)
{
    record.error.code = code;
    record.error.message = std::move(message);
}

bool copyMetadataString(const char* source, QString& destination)
{
    if (source == nullptr)
    {
        return false;
    }

    destination = QString::fromUtf8(source);
    return !destination.trimmed().isEmpty();
}

} // namespace

struct PluginManager::ActivePlugin
{
    NativePluginLoader loader;
    std::size_t recordIndex = 0;
    bool initInvoked = false;
    bool shutdownCalled = false;
    bool registrationStarted = false;
};

bool PluginManagerError::isError() const noexcept
{
    return code != PluginManagerErrorCode::None;
}

PluginManager::PluginManager(
    HostApi& hostApi,
    PluginRegistry& registry)
    : m_hostApi(hostApi)
    , m_registry(registry)
{
}

PluginManager::PluginManager(
    HostApi& hostApi,
    PluginRegistry& registry,
    QString discoveryDirectory)
    : m_hostApi(hostApi)
    , m_registry(registry)
    , m_manifestReader(std::move(discoveryDirectory))
{
}

PluginManager::~PluginManager() noexcept
{
    shutdownAll();
}

void PluginManager::loadAll()
{
    if (m_loadAttempted || m_shutdown)
    {
        return;
    }
    m_loadAttempted = true;

    const auto manifestResults = m_manifestReader.discover();
    m_records.reserve(manifestResults.size());
    m_activePlugins.reserve(
        static_cast<std::size_t>(manifestResults.size()));

    for (const auto& manifestResult : manifestResults)
    {
        PluginManagerRecord record;
        record.manifestPath = manifestResult.xmlPath;

        if (!manifestResult.isValid())
        {
            setError(
                record,
                PluginManagerErrorCode::InvalidManifest,
                manifestResult.error.message);
            record.error.manifestCode = manifestResult.error.code;
            m_records.append(std::move(record));
            continue;
        }

        record.dllPath = manifestResult.manifest.dllPath();
        record.dllDirectory = manifestResult.manifest.dllDirectory();
        record.state = PluginLifecycleState::Validated;
        m_records.append(std::move(record));

        const auto recordIndex =
            static_cast<std::size_t>(m_records.size() - 1);
        loadManifest(manifestResult, recordIndex);
    }
}

void PluginManager::shutdownAll() noexcept
{
    if (m_shutdown)
    {
        return;
    }
    m_shutdown = true;

    for (auto iterator = m_activePlugins.rbegin();
         iterator != m_activePlugins.rend();
         ++iterator)
    {
        auto& plugin = **iterator;
        auto& record = m_records[static_cast<int>(plugin.recordIndex)];
        const bool shutdownSucceeded = invokeShutdown(plugin, record);
        plugin.loader.unload();

        if (!shutdownSucceeded && !record.error.isError())
        {
            record.error.code = PluginManagerErrorCode::ShutdownFailed;
            try
            {
                record.error.message =
                    QStringLiteral("插件 shutdown 抛出了异常");
            }
            catch (...)
            {
            }
        }
    }
    m_activePlugins.clear();
}

const QVector<PluginManagerRecord>& PluginManager::records() const noexcept
{
    return m_records;
}

bool PluginManager::hasLoaded() const noexcept
{
    return m_loadAttempted;
}

bool PluginManager::isShutdown() const noexcept
{
    return m_shutdown;
}

bool PluginManager::isPluginActive(const QString& pluginId) const noexcept
{
    if (m_shutdown || pluginId.isEmpty())
    {
        return false;
    }
    for (const auto& record : m_records)
    {
        if (record.pluginId == pluginId &&
            record.state == PluginLifecycleState::Active)
        {
            return true;
        }
    }
    return false;
}

void PluginManager::loadManifest(
    const PluginManifestReadResult& manifestResult,
    std::size_t recordIndex)
{
    auto& record = m_records[static_cast<int>(recordIndex)];
    std::unique_ptr<ActivePlugin> plugin;

    try
    {
        plugin = std::make_unique<ActivePlugin>();
        plugin->recordIndex = recordIndex;

        const YiCadHostApi* host = m_hostApi.api();
        if (host == nullptr)
        {
            setError(
                record,
                PluginManagerErrorCode::HostApiUnavailable,
                QStringLiteral("宿主 API 当前不可用"));
            return;
        }
        if (host->structSize < AbiHeaderSize ||
            host->abiVersion < YICAD_PLUGIN_ABI_MIN_VERSION ||
            host->abiVersion > YICAD_PLUGIN_ABI_MAX_VERSION)
        {
            setError(
                record,
                PluginManagerErrorCode::HostApiLayoutMismatch,
                QStringLiteral("宿主 API 的 ABI 版本或结构大小不匹配"));
            return;
        }

        const auto loadResult = plugin->loader.load(
            manifestResult.manifest.dllPath());
        if (!loadResult.isSuccess())
        {
            setError(
                record,
                PluginManagerErrorCode::LoadFailed,
                loadResult.message);
            record.error.loadCode = loadResult.code;
            return;
        }
        record.state = PluginLifecycleState::Loaded;

        try
        {
            record.pluginAbiVersion =
                plugin->loader.abiVersionFunction()();
        }
        catch (...)
        {
            setError(
                record,
                PluginManagerErrorCode::AbiVersionQueryFailed,
                QStringLiteral("插件 ABI 版本入口抛出了异常"));
            return;
        }

        if (record.pluginAbiVersion < YICAD_PLUGIN_ABI_MIN_VERSION)
        {
            setError(
                record,
                PluginManagerErrorCode::AbiVersionMismatch,
                QStringLiteral("插件 ABI 版本 %1 低于宿主最低支持版本 %2")
                    .arg(record.pluginAbiVersion)
                    .arg(YICAD_PLUGIN_ABI_MIN_VERSION));
            return;
        }
        record.negotiatedAbiVersion =
            record.pluginAbiVersion < host->abiVersion
            ? record.pluginAbiVersion
            : host->abiVersion;

        if (!m_registry.beginRegistration())
        {
            setError(
                record,
                PluginManagerErrorCode::RegistrationBeginFailed,
                m_registry.lastError().message);
            record.error.registryCode = m_registry.lastError().code;
            return;
        }
        plugin->registrationStarted = true;

        YiCadPluginApi pluginApi{};
        pluginApi.structSize = PluginApiCapacity;
        pluginApi.abiVersion = record.negotiatedAbiVersion;

        YiCadResult initResult = YICAD_FAILURE;
        plugin->initInvoked = true;
        record.initInvoked = true;
        try
        {
            initResult = plugin->loader.initFunction()(host, &pluginApi);
        }
        catch (...)
        {
            setError(
                record,
                PluginManagerErrorCode::InitThrewException,
                QStringLiteral("插件 init 抛出了异常"));
            cleanupFailedPlugin(*plugin, record);
            return;
        }

        if (initResult != YICAD_SUCCESS)
        {
            setError(
                record,
                PluginManagerErrorCode::InitFailed,
                QStringLiteral("插件 init 返回失败"));
            cleanupFailedPlugin(*plugin, record);
            return;
        }
        record.state = PluginLifecycleState::Initialized;

        if (pluginApi.structSize < PluginApiV1RequiredSize)
        {
            setError(
                record,
                PluginManagerErrorCode::PluginApiLayoutMismatch,
                QStringLiteral("插件输出结构未覆盖 ABI v1 元数据"));
            cleanupFailedPlugin(*plugin, record);
            return;
        }
        if (pluginApi.structSize > PluginApiCapacity)
        {
            setError(
                record,
                PluginManagerErrorCode::PluginApiLayoutMismatch,
                QStringLiteral("插件输出结构大小超过宿主提供的容量"));
            cleanupFailedPlugin(*plugin, record);
            return;
        }
        if (pluginApi.abiVersion != record.negotiatedAbiVersion)
        {
            setError(
                record,
                PluginManagerErrorCode::PluginApiVersionMismatch,
                QStringLiteral("插件未确认宿主协商的 ABI 版本 %1")
                    .arg(record.negotiatedAbiVersion));
            cleanupFailedPlugin(*plugin, record);
            return;
        }

        if (!copyMetadataString(pluginApi.pluginId, record.pluginId))
        {
            setError(
                record,
                PluginManagerErrorCode::InvalidPluginId,
                QStringLiteral("插件 ID 不能为空"));
            cleanupFailedPlugin(*plugin, record);
            return;
        }
        if (!copyMetadataString(pluginApi.pluginName, record.pluginName))
        {
            setError(
                record,
                PluginManagerErrorCode::InvalidPluginName,
                QStringLiteral("插件名称不能为空"));
            cleanupFailedPlugin(*plugin, record);
            return;
        }
        if (!copyMetadataString(pluginApi.pluginVersion, record.pluginVersion))
        {
            setError(
                record,
                PluginManagerErrorCode::InvalidPluginVersion,
                QStringLiteral("插件版本不能为空"));
            cleanupFailedPlugin(*plugin, record);
            return;
        }

        const PluginRecord registryRecord{
            record.pluginId,
            record.pluginName,
            record.pluginVersion,
            record.dllPath,
            record.dllDirectory};

        if (!m_registry.commitRegistration(registryRecord))
        {
            plugin->registrationStarted = false;
            setError(
                record,
                PluginManagerErrorCode::RegistryCommitFailed,
                m_registry.lastError().message);
            record.error.registryCode = m_registry.lastError().code;
            cleanupFailedPlugin(*plugin, record);
            return;
        }
        plugin->registrationStarted = false;

        record.state = PluginLifecycleState::Active;
        m_activePlugins.push_back(std::move(plugin));
    }
    catch (...)
    {
        if (!record.error.isError())
        {
            setError(
                record,
                PluginManagerErrorCode::UnexpectedFailure,
                QStringLiteral("插件加载过程中发生未预期异常"));
        }
        if (plugin != nullptr)
        {
            cleanupFailedPlugin(*plugin, record);
        }
    }
}

bool PluginManager::invokeShutdown(
    ActivePlugin& plugin,
    PluginManagerRecord& record) noexcept
{
    if (!plugin.initInvoked || plugin.shutdownCalled)
    {
        return true;
    }

    plugin.shutdownCalled = true;
    record.shutdownCalled = true;
    record.state = PluginLifecycleState::Shutdown;
    try
    {
        plugin.loader.shutdownFunction()();
        record.shutdownSucceeded = true;
        return true;
    }
    catch (...)
    {
        record.shutdownSucceeded = false;
        return false;
    }
}

void PluginManager::cleanupFailedPlugin(
    ActivePlugin& plugin,
    PluginManagerRecord& record) noexcept
{
    if (plugin.registrationStarted)
    {
        m_registry.rollbackRegistration();
        plugin.registrationStarted = false;
    }

    invokeShutdown(plugin, record);
    plugin.loader.unload();
}
