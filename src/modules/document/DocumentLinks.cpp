#include "DocumentLinks.h"

#include "DocumentFormat.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QStringList>

namespace mirante::modules::document {

QUrl rawGitHubUrl(const QUrl &url)
{
    if (url.host().compare(QLatin1String("github.com"), Qt::CaseInsensitive) != 0
        && url.host().compare(QLatin1String("www.github.com"), Qt::CaseInsensitive) != 0)
        return url;
    // /<owner>/<repo>/blob/<ref>/<path...>
    const QStringList parts = url.path(QUrl::FullyEncoded).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() < 5 || parts.at(2) != QLatin1String("blob"))
        return url;
    QStringList rest = parts.mid(3);
    QUrl raw;
    raw.setScheme(QStringLiteral("https"));
    raw.setHost(QStringLiteral("raw.githubusercontent.com"));
    raw.setPath(QLatin1Char('/') + parts.at(0) + QLatin1Char('/') + parts.at(1) + QLatin1Char('/')
                    + rest.join(QLatin1Char('/')),
                QUrl::StrictMode);
    return raw;
}

std::optional<UrlMatch> urlMatch(const QUrl &url)
{
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https")))
        return std::nullopt;
    const QString fileName = url.fileName();
    if (formatFromName(fileName) == Format::Unknown)
        return std::nullopt;
    UrlMatch match;
    match.config.insert(QStringLiteral("url"), rawGitHubUrl(url).toString());
    match.name = fileName;
    return match;
}

std::optional<QString> pathInside(const QString &folder, const QString &file)
{
    if (folder.isEmpty())
        return std::nullopt;
    const QString relative = QDir(QDir::cleanPath(folder)).relativeFilePath(QDir::cleanPath(file));
    if (relative.isEmpty() || relative == QLatin1String(".") || relative == QLatin1String("..")
        || relative.startsWith(QLatin1String("../")) || QDir::isAbsolutePath(relative))
        return std::nullopt;
    return QDir::fromNativeSeparators(relative);
}

std::optional<FileMatch> projectFileMatch(const FileDrop &drop)
{
    if (formatFromName(drop.path) == Format::Unknown)
        return std::nullopt;
    for (const auto &[id, folder] : drop.folders) {
        if (const std::optional<QString> path = pathInside(folder, drop.path)) {
            FileMatch match;
            match.config.insert(QStringLiteral("folder"), id);
            match.config.insert(QStringLiteral("path"), *path);
            match.name = QFileInfo(drop.path).fileName();
            return match;
        }
    }
    return std::nullopt;
}

std::optional<FileMatch> workspaceFileMatch(const FileDrop &drop)
{
    if (formatFromName(drop.path) == Format::Unknown)
        return std::nullopt;
    for (const auto &folder : drop.folders) {
        if (pathInside(folder.second, drop.path))
            return std::nullopt;
    }
    FileMatch match;
    match.config.insert(QStringLiteral("originalName"), QFileInfo(drop.path).fileName());
    match.name = QFileInfo(drop.path).fileName();
    match.copyKey = QStringLiteral("file");
    return match;
}

} // namespace mirante::modules::document
