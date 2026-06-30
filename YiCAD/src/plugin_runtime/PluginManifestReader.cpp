#include "PluginManifestReader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QXmlStreamAttribute>
#include <QXmlStreamReader>

#include <algorithm>
#include <utility>

namespace
{

void setError(
    PluginManifestReadResult& result,
    PluginManifestErrorCode code,
    QString message,
    qint64 line = -1,
    qint64 column = -1)
{
    if (result.error.code != PluginManifestErrorCode::None)
    {
        return;
    }

    result.error.code = code;
    result.error.message = std::move(message);
    result.error.line = line;
    result.error.column = column;
}

QString absoluteCleanPath(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

} // namespace

const QString PluginManifestReader::ProductionDiscoveryDirectory =
    QStringLiteral("C:\\ProgramData\\YiCAD\\plugins");

bool PluginManifestReadResult::isValid() const noexcept
{
    return error.code == PluginManifestErrorCode::None;
}

PluginManifestReader::PluginManifestReader()
    : PluginManifestReader(ProductionDiscoveryDirectory)
{
}

PluginManifestReader::PluginManifestReader(QString discoveryDirectory)
{
    if (!discoveryDirectory.trimmed().isEmpty())
    {
        m_discoveryDirectory = QDir::cleanPath(
            std::move(discoveryDirectory));
    }
}

const QString& PluginManifestReader::discoveryDirectory() const noexcept
{
    return m_discoveryDirectory;
}

QVector<PluginManifestReadResult> PluginManifestReader::discover() const
{
    QVector<PluginManifestReadResult> results;

    if (m_discoveryDirectory.trimmed().isEmpty())
    {
        return results;
    }

    const QDir discoveryDirectory(m_discoveryDirectory);
    if (!discoveryDirectory.exists())
    {
        return results;
    }

    QFileInfoList xmlFiles = discoveryDirectory.entryInfoList(
        QStringList{QStringLiteral("*.xml")},
        QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
        QDir::NoSort);

    std::sort(
        xmlFiles.begin(),
        xmlFiles.end(),
        [](const QFileInfo& left, const QFileInfo& right)
        {
            const int caseInsensitiveOrder = QString::compare(
                left.fileName(),
                right.fileName(),
                Qt::CaseInsensitive);
            if (caseInsensitiveOrder != 0)
            {
                return caseInsensitiveOrder < 0;
            }

            return QString::compare(
                       left.fileName(),
                       right.fileName(),
                       Qt::CaseSensitive) < 0;
        });

    results.reserve(xmlFiles.size());
    for (const QFileInfo& xmlFile : xmlFiles)
    {
        results.append(readManifest(xmlFile.absoluteFilePath()));
    }

    return results;
}

PluginManifestReadResult PluginManifestReader::readManifest(
    const QString& xmlPath) const
{
    PluginManifestReadResult result;
    result.xmlPath = absoluteCleanPath(xmlPath);

    const QFileInfo xmlFileInfo(result.xmlPath);
    const QString canonicalXmlPath = xmlFileInfo.canonicalFilePath();
    if (!canonicalXmlPath.isEmpty())
    {
        result.xmlPath = QDir::cleanPath(canonicalXmlPath);
    }

    QFile xmlFile(result.xmlPath);
    if (!xmlFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        setError(
            result,
            PluginManifestErrorCode::CannotOpenFile,
            QStringLiteral("Cannot open plugin manifest: %1")
                .arg(xmlFile.errorString()));
        return result;
    }

    QXmlStreamReader reader(&xmlFile);
    bool rootFound = false;
    int depth = 0;
    QString dllAttribute;

    while (!reader.atEnd())
    {
        const QXmlStreamReader::TokenType token = reader.readNext();

        if (token == QXmlStreamReader::StartElement)
        {
            if (!rootFound)
            {
                rootFound = true;
                depth = 1;

                if (reader.qualifiedName() != QStringLiteral("plugin") ||
                    !reader.namespaceUri().isEmpty())
                {
                    setError(
                        result,
                        PluginManifestErrorCode::InvalidRootElement,
                        QStringLiteral("The root element must be <plugin>."),
                        reader.lineNumber(),
                        reader.columnNumber());
                    continue;
                }

                int dllAttributeCount = 0;
                bool hasUnexpectedAttribute = false;
                const QXmlStreamAttributes attributes = reader.attributes();
                for (const QXmlStreamAttribute& attribute : attributes)
                {
                    if (attribute.qualifiedName() == QStringLiteral("dll") &&
                        attribute.namespaceUri().isEmpty())
                    {
                        ++dllAttributeCount;
                        dllAttribute = attribute.value().toString();
                    }
                    else
                    {
                        hasUnexpectedAttribute = true;
                    }
                }

                if (dllAttributeCount == 0)
                {
                    setError(
                        result,
                        PluginManifestErrorCode::MissingDllAttribute,
                        QStringLiteral("The <plugin> element is missing its dll attribute."),
                        reader.lineNumber(),
                        reader.columnNumber());
                }
                else if (dllAttributeCount > 1)
                {
                    setError(
                        result,
                        PluginManifestErrorCode::DuplicateDllAttribute,
                        QStringLiteral("The <plugin> element has more than one dll attribute."),
                        reader.lineNumber(),
                        reader.columnNumber());
                }
                else if (dllAttribute.trimmed().isEmpty())
                {
                    setError(
                        result,
                        PluginManifestErrorCode::EmptyDllAttribute,
                        QStringLiteral("The dll attribute cannot be empty."),
                        reader.lineNumber(),
                        reader.columnNumber());
                }
                else if (hasUnexpectedAttribute ||
                         !reader.namespaceDeclarations().isEmpty())
                {
                    setError(
                        result,
                        PluginManifestErrorCode::UnexpectedAttribute,
                        QStringLiteral("The <plugin> element may only have one dll attribute."),
                        reader.lineNumber(),
                        reader.columnNumber());
                }
            }
            else
            {
                ++depth;
                setError(
                    result,
                    PluginManifestErrorCode::UnexpectedElement,
                    QStringLiteral("The <plugin> element cannot contain child elements."),
                    reader.lineNumber(),
                    reader.columnNumber());
            }
        }
        else if (token == QXmlStreamReader::EndElement)
        {
            --depth;
        }
        else if (token == QXmlStreamReader::Characters &&
                 rootFound && depth > 0 && !reader.isWhitespace())
        {
            setError(
                result,
                PluginManifestErrorCode::UnexpectedText,
                QStringLiteral("The <plugin> element cannot contain text."),
                reader.lineNumber(),
                reader.columnNumber());
        }
        else if (token == QXmlStreamReader::DTD)
        {
            setError(
                result,
                PluginManifestErrorCode::UnexpectedElement,
                QStringLiteral("Plugin manifests cannot contain a DTD."),
                reader.lineNumber(),
                reader.columnNumber());
        }
    }

    if (reader.hasError())
    {
        result.error.code = PluginManifestErrorCode::MalformedXml;
        result.error.message = reader.errorString();
        result.error.line = reader.lineNumber();
        result.error.column = reader.columnNumber();
        return result;
    }

    if (!rootFound)
    {
        setError(
            result,
            PluginManifestErrorCode::MissingRootElement,
            QStringLiteral("The plugin manifest has no root element."));
    }

    if (!result.isValid())
    {
        return result;
    }

    const QString declaredDllPath = dllAttribute.trimmed();
    const QString resolvedDllPath = QDir::isAbsolutePath(declaredDllPath)
                                        ? declaredDllPath
                                        : xmlFileInfo.absoluteDir().absoluteFilePath(
                                              declaredDllPath);
    const QString cleanedDllPath = QDir::cleanPath(resolvedDllPath);
    const QFileInfo dllFileInfo(cleanedDllPath);

    if (dllFileInfo.suffix().compare(
            QStringLiteral("dll"),
            Qt::CaseInsensitive) != 0)
    {
        setError(
            result,
            PluginManifestErrorCode::InvalidDllExtension,
            QStringLiteral("The declared plugin path must have a .dll extension."));
        return result;
    }

    if (!dllFileInfo.exists())
    {
        setError(
            result,
            PluginManifestErrorCode::DllDoesNotExist,
            QStringLiteral("The declared plugin DLL does not exist: %1")
                .arg(cleanedDllPath));
        return result;
    }

    if (!dllFileInfo.isFile())
    {
        setError(
            result,
            PluginManifestErrorCode::DllPathIsNotFile,
            QStringLiteral("The declared plugin DLL is not a file: %1")
                .arg(cleanedDllPath));
        return result;
    }

    const QString canonicalDllPath = dllFileInfo.canonicalFilePath();
    if (canonicalDllPath.isEmpty() ||
        !QFileInfo(canonicalDllPath).isAbsolute())
    {
        setError(
            result,
            PluginManifestErrorCode::DllPathNormalizationFailed,
            QStringLiteral("The declared plugin DLL path could not be normalized."));
        return result;
    }

    const QFileInfo canonicalDllFileInfo(canonicalDllPath);
    if (canonicalDllFileInfo.suffix().compare(
            QStringLiteral("dll"),
            Qt::CaseInsensitive) != 0)
    {
        setError(
            result,
            PluginManifestErrorCode::InvalidDllExtension,
            QStringLiteral("The normalized plugin path must have a .dll extension."));
        return result;
    }

    const QString normalizedDllPath = QDir::cleanPath(canonicalDllPath);
    result.manifest = PluginManifest(
        result.xmlPath,
        normalizedDllPath,
        canonicalDllFileInfo.absolutePath());
    return result;
}
