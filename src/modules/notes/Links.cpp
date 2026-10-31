#include "Links.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QStringList>
#include <QUrl>

#include <utility>

namespace mirante::modules::notes {

namespace {

const QString kKindUrl = QStringLiteral("url");
const QString kKindProject = QStringLiteral("project");
const QString kKindWorkspace = QStringLiteral("workspace");

QString trimmed(const QJsonObject &link, const QString &key)
{
    return link.value(key).toString().trimmed();
}

QString withForwardSlashes(QString path)
{
    return path.replace(QLatin1Char('\\'), QLatin1Char('/'));
}

} // namespace

QString linkKindId(LinkKind kind)
{
    switch (kind) {
    case LinkKind::Url:
        return kKindUrl;
    case LinkKind::Project:
        return kKindProject;
    case LinkKind::Workspace:
        return kKindWorkspace;
    }
    return kKindUrl;
}

LinkKind linkKind(const QJsonObject &link)
{
    const QString kind = link.value(kLinkKind).toString();
    if (kind == kKindProject)
        return LinkKind::Project;
    if (kind == kKindWorkspace)
        return LinkKind::Workspace;
    return LinkKind::Url;
}

QList<QJsonObject> linksOf(const QJsonObject &config)
{
    QList<QJsonObject> links;
    const QJsonArray array = config.value(kLinks).toArray();
    for (const QJsonValue &value : array) {
        if (value.isObject())
            links << value.toObject();
    }
    return links;
}

QString linkTitle(const QJsonObject &link)
{
    const QString title = trimmed(link, kLinkTitle);
    if (!title.isEmpty())
        return title;
    switch (linkKind(link)) {
    case LinkKind::Url:
        return trimmed(link, kLinkUrl);
    case LinkKind::Project: {
        const QString path = withForwardSlashes(trimmed(link, kLinkPath));
        if (path.isEmpty())
            return QCoreApplication::translate("notes", "Project folder");
        return path.section(QLatin1Char('/'), -1, -1, QString::SectionSkipEmpty);
    }
    case LinkKind::Workspace: {
        const QString original = trimmed(link, kLinkOriginalName);
        return original.isEmpty() ? trimmed(link, kLinkFile) : original;
    }
    }
    return {};
}

bool isOpenableAddress(const QString &address)
{
    const QUrl url(address.trimmed(), QUrl::StrictMode);
    if (!url.isValid())
        return false;
    if (url.scheme() == QLatin1String("mailto"))
        return !url.path().isEmpty();
    return (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"))
           && !url.host().isEmpty();
}

bool isInsideFolder(const QString &path)
{
    const QString forward = withForwardSlashes(path.trimmed());
    // The colon also refuses "C:file" on Windows, which is relative to a drive.
    if (forward.startsWith(QLatin1Char('/')) || forward.contains(QLatin1Char(':')) || QDir::isAbsolutePath(forward))
        return false;
    return !forward.split(QLatin1Char('/')).contains(QStringLiteral(".."));
}

QString linkProblem(const QJsonObject &link)
{
    switch (linkKind(link)) {
    case LinkKind::Url: {
        const QString address = trimmed(link, kLinkUrl);
        if (address.isEmpty())
            return QCoreApplication::translate("notes", "Enter the address.");
        if (!isOpenableAddress(address))
            return QCoreApplication::translate("notes", "The address must start with http://, https:// or mailto:.");
        return {};
    }
    case LinkKind::Project:
        if (trimmed(link, kLinkFolder).isEmpty())
            return QCoreApplication::translate("notes", "Choose the folder.");
        if (!isInsideFolder(trimmed(link, kLinkPath)))
            return QCoreApplication::translate("notes", "The path must be relative and stay inside the folder.");
        return {};
    case LinkKind::Workspace:
        if (trimmed(link, kLinkFile).isEmpty())
            return QCoreApplication::translate("notes", "Choose the file.");
        return {};
    }
    return {};
}

QJsonObject cleanedLink(const QJsonObject &link)
{
    QJsonObject cleaned = link;
    const LinkKind kind = linkKind(link);
    cleaned.insert(kLinkKind, linkKindId(kind));

    QStringList kept{kLinkTitle, kLinkDescription};
    QStringList dropped;
    switch (kind) {
    case LinkKind::Url:
        kept << kLinkUrl;
        dropped << kLinkFolder << kLinkPath << kLinkFile << kLinkOriginalName;
        break;
    case LinkKind::Project:
        kept << kLinkFolder << kLinkPath;
        dropped << kLinkUrl << kLinkFile << kLinkOriginalName;
        break;
    case LinkKind::Workspace:
        kept << kLinkFile << kLinkOriginalName;
        dropped << kLinkUrl << kLinkFolder << kLinkPath;
        break;
    }
    for (const QString &key : std::as_const(dropped))
        cleaned.remove(key);
    for (const QString &key : std::as_const(kept)) {
        const QString value = trimmed(link, key);
        if (value.isEmpty())
            cleaned.remove(key);
        else
            cleaned.insert(key, key == kLinkPath ? withForwardSlashes(value) : value);
    }
    return cleaned;
}

} // namespace mirante::modules::notes
