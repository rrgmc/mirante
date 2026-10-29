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

std::optional<UrlMatch> matchUrl(const QUrl &url)
{
    const QUrl page = pageUrl(QJsonObject{{QStringLiteral("url"), url.toString()}});
    if (page.isEmpty())
        return std::nullopt;
    QString host = page.host();
    if (host.startsWith(QLatin1String("www."), Qt::CaseInsensitive))
        host.remove(0, 4);

    UrlMatch match;
    match.config.insert(QStringLiteral("url"), page.toString());
    match.name = host;
    match.generic = true;
    return match;
}

} // namespace mirante::modules::web
