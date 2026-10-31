#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace mirante::modules::notes {

// The keys of the Links type's configuration and of each link in it. See
// docs/item-types.md#links.
inline const QString kLinks = QStringLiteral("links");
inline const QString kLinkKind = QStringLiteral("kind");
inline const QString kLinkTitle = QStringLiteral("title");
inline const QString kLinkDescription = QStringLiteral("description");
inline const QString kLinkUrl = QStringLiteral("url");
inline const QString kLinkFolder = QStringLiteral("folder");
inline const QString kLinkPath = QStringLiteral("path");
inline const QString kLinkFile = QStringLiteral("file");
inline const QString kLinkOriginalName = QStringLiteral("originalName");

// Where a link leads: a web or mail address, a file in one of the project's
// folders, or a file in the data folder's files/.
enum class LinkKind { Url, Project, Workspace };

// The stored value of a kind.
QString linkKindId(LinkKind kind);
// The kind of a link. One without a known kind is a web address.
LinkKind linkKind(const QJsonObject &link);

// The links of an item's configuration, in order.
QList<QJsonObject> linksOf(const QJsonObject &config);

// The link's title, or a name made from where it leads. Empty only for a web
// address with no address yet.
QString linkTitle(const QJsonObject &link);

// True for an address that a link may open: http, https or mailto.
bool isOpenableAddress(const QString &address);

// True for a relative path that stays inside its folder: never absolute, with
// no ".." part. A link's stored data syncs, so the view checks it too.
bool isInsideFolder(const QString &path);

// What is wrong with a link, for the user, or empty.
QString linkProblem(const QJsonObject &link);

// The link as it is stored: trimmed, without empty values or the keys of its
// other kinds, and with "/" in its path. Other keys stay.
QJsonObject cleanedLink(const QJsonObject &link);

} // namespace mirante::modules::notes
