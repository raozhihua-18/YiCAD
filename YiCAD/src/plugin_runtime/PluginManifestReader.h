#ifndef PLUGIN_MANIFEST_READER_H
#define PLUGIN_MANIFEST_READER_H

#include "PluginManifest.h"

#include <QString>
#include <QVector>
#include <QtGlobal>

enum class PluginManifestErrorCode
{
    None,
    CannotOpenFile,
    MalformedXml,
    MissingRootElement,
    InvalidRootElement,
    MissingDllAttribute,
    DuplicateDllAttribute,
    EmptyDllAttribute,
    UnexpectedAttribute,
    UnexpectedElement,
    UnexpectedText,
    InvalidDllExtension,
    DllDoesNotExist,
    DllPathIsNotFile,
    DllPathNormalizationFailed
};

struct PluginManifestError
{
    PluginManifestErrorCode code = PluginManifestErrorCode::None;
    QString message;
    qint64 line = -1;
    qint64 column = -1;
};

struct PluginManifestReadResult
{
    QString xmlPath;
    PluginManifest manifest;
    PluginManifestError error;

    bool isValid() const noexcept;
};

class PluginManifestReader
{
public:
    static const QString ProductionDiscoveryDirectory;

    PluginManifestReader();
    explicit PluginManifestReader(QString discoveryDirectory);

    const QString& discoveryDirectory() const noexcept;

    QVector<PluginManifestReadResult> discover() const;
    PluginManifestReadResult readManifest(const QString& xmlPath) const;

private:
    QString m_discoveryDirectory;
};

#endif // PLUGIN_MANIFEST_READER_H
