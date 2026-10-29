#include "items/UrlMatches.h"

#include "core/Entities.h"
#include "items/ItemTypeRegistry.h"

#include <mirante/ItemType.h>

#include <algorithm>

namespace mirante::urlmatches {

QUrl webAddress(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed.contains(QLatin1Char(' ')) || trimmed.contains(QLatin1Char('\n')))
        return {};
    const QUrl url(trimmed, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty()
        || (scheme != QLatin1String("http") && scheme != QLatin1String("https")))
        return {};
    return url;
}

std::vector<TypeMatch> match(const ItemTypeRegistry &types, const QUrl &url,
                             const std::vector<Connection> &connections)
{
    std::vector<TypeMatch> result;
    if (url.isEmpty())
        return result;
    for (const ItemType *type : types.types()) {
        const QStringList kinds = type->connectionKinds();
        // The kinds in the type's order of preference, so the first
        // connection found for a base URL is the preferred one.
        std::vector<const Connection *> usable;
        for (const QString &kind : kinds) {
            for (const Connection &connection : connections) {
                if (connection.kind == kind)
                    usable.push_back(&connection);
            }
        }
        QList<QUrl> baseUrls;
        for (const Connection *connection : usable)
            baseUrls << QUrl(connection->baseUrl);

        std::optional<UrlMatch> found = type->fromUrl(url, baseUrls);
        if (!found)
            continue;
        TypeMatch entry;
        entry.type = type;
        entry.match = *found;
        if (!found->baseUrl.isEmpty()) {
            for (const Connection *connection : usable) {
                // The same server and path, ignoring slashes at the end.
                const std::optional<QString> below = pathBelow(found->baseUrl, QUrl(connection->baseUrl));
                if (below && below->isEmpty()) {
                    entry.connectionId = connection->id;
                    break;
                }
            }
        }
        result.push_back(std::move(entry));
    }
    // The registry lists types by name; generic types go last.
    std::stable_partition(result.begin(), result.end(), [](const TypeMatch &entry) { return !entry.match.generic; });
    return result;
}

} // namespace mirante::urlmatches
