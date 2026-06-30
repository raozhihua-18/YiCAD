#ifndef PLUGIN_MANIFEST_H
#define PLUGIN_MANIFEST_H

#include <QString>

class PluginManifest
{
public:
    PluginManifest() = default;
    PluginManifest(QString xmlPath, QString dllPath, QString dllDirectory);

    const QString& xmlPath() const noexcept;
    const QString& dllPath() const noexcept;
    const QString& dllDirectory() const noexcept;

private:
    QString m_xmlPath;
    QString m_dllPath;
    QString m_dllDirectory;
};

#endif // PLUGIN_MANIFEST_H
