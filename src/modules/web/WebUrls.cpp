#include "WebUrls.h"

#include <QString>

namespace mirante::modules::web {

QUrl pageUrl(const QJsonObject &config)
{
    const QUrl url(config.value(QStringLiteral("url")).toString().trimmed(), QUrl::StrictMode);
    if (!url.isValid() || url.host().isEmpty())
        return {};
    if (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https"))
        return {};
    return url;
}

} // namespace mirante::modules::web
