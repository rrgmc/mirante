#pragma once

#include <QJsonObject>
#include <QString>
#include <QUrl>

#include <optional>

namespace mirante {

// What an item type makes of a web address that the user pasted, returned by
// ItemType::fromUrl(). See docs/modules.md#items-from-a-link and
// docs/decisions/0049-items-from-a-pasted-link.md.
struct UrlMatch
{
    // The configuration that shows the address, with the keys of the type's
    // fields. Keys left out take the fields' defaults.
    QJsonObject config;

    // A name for the new item, such as "acme/backend!42". Empty for none.
    QString name;

    // The base URL of the server the address belongs to, such as
    // "https://gitlab.example.org". For information only: the core does not
    // use it. Empty for a type that talks to no server.
    QUrl baseUrl;

    // True for a type that takes any address, such as Web page. The item
    // dialog lists such types after the others.
    bool generic = false;
};

// The path of url below base, without slashes at its start and end, if url is
// on base's server (the same scheme, host and port, ignoring case) and its path
// starts with base's path. Null otherwise. For example, the path of
// https://example.org/gitlab/a/b below https://example.org/gitlab/ is "a/b".
inline std::optional<QString> pathBelow(const QUrl &url, const QUrl &base)
{
    if (!url.isValid() || !base.isValid() || base.host().isEmpty())
        return std::nullopt;
    if (url.scheme().compare(base.scheme(), Qt::CaseInsensitive) != 0
        || url.host().compare(base.host(), Qt::CaseInsensitive) != 0
        || url.port(url.scheme() == QLatin1String("http") ? 80 : 443)
               != base.port(base.scheme() == QLatin1String("http") ? 80 : 443))
        return std::nullopt;

    auto trimmed = [](QString path) {
        while (path.startsWith(QLatin1Char('/')))
            path.remove(0, 1);
        while (path.endsWith(QLatin1Char('/')))
            path.chop(1);
        return path;
    };
    const QString basePath = trimmed(base.path());
    const QString path = trimmed(url.path());
    if (basePath.isEmpty())
        return path;
    if (path == basePath)
        return QString();
    if (!path.startsWith(basePath + QLatin1Char('/')))
        return std::nullopt;
    return path.mid(basePath.size() + 1);
}

} // namespace mirante
