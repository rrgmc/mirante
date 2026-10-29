#include "items/UrlMatches.h"

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

std::vector<TypeMatch> match(const ItemTypeRegistry &types, const QUrl &url, const QString &profileId)
{
    std::vector<TypeMatch> result;
    if (url.isEmpty())
        return result;
    for (const ItemType *type : types.types()) {
        std::optional<UrlMatch> found = type->fromUrl(url, profileId);
        if (!found)
            continue;
        TypeMatch entry;
        entry.type = type;
        entry.match = *found;
        result.push_back(std::move(entry));
    }
    // The registry lists types by name; generic types go last.
    std::stable_partition(result.begin(), result.end(), [](const TypeMatch &entry) { return !entry.match.generic; });
    return result;
}

} // namespace mirante::urlmatches
