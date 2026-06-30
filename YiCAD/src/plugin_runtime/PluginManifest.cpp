#include "PluginManifest.h"

#include <utility>

PluginManifest::PluginManifest(
    QString xmlPath,
    QString dllPath,
    QString dllDirectory)
    : m_xmlPath(std::move(xmlPath)),
      m_dllPath(std::move(dllPath)),
      m_dllDirectory(std::move(dllDirectory))
{
}

const QString& PluginManifest::xmlPath() const noexcept
{
    return m_xmlPath;
}

const QString& PluginManifest::dllPath() const noexcept
{
    return m_dllPath;
}

const QString& PluginManifest::dllDirectory() const noexcept
{
    return m_dllDirectory;
}
