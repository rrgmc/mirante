#include "web/DashSchemeHandler.h"

#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>

namespace mirante {

namespace dash {

namespace {

bool registered = false;

} // namespace

QByteArray schemeName() { return QByteArrayLiteral("dash"); }

void registerScheme()
{
    QWebEngineUrlScheme scheme(schemeName());
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    // A secure context that other schemes' pages cannot load.
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme | QWebEngineUrlScheme::LocalScheme);
    QWebEngineUrlScheme::registerScheme(scheme);
    registered = true;
}

bool isRegistered() { return registered; }

std::optional<QString> resourceFor(const QUrl &url)
{
    if (url.scheme() != QLatin1String(schemeName()) || url.host() != QLatin1String("panels"))
        return std::nullopt;
    const QString path = url.path(QUrl::FullyDecoded);
    if (!path.startsWith(QLatin1Char('/')) || path.contains(QLatin1Char('\\')) || path.contains(QLatin1Char(':')))
        return std::nullopt;
    // Every part a plain name: no "..", no ".", and no empty part, which also
    // refuses a folder.
    const QStringList parts = path.mid(1).split(QLatin1Char('/'));
    for (const QString &part : parts) {
        if (part.isEmpty() || part == QLatin1String(".") || part == QLatin1String(".."))
            return std::nullopt;
    }
    // The page side of QWebChannel, from Qt's own resources, since the
    // policy allows dash: only.
    if (path == QLatin1String("/common/qwebchannel.js"))
        return QStringLiteral(":/qtwebchannel/qwebchannel.js");
    return QStringLiteral(":/panels") + path;
}

QByteArray contentTypeFor(const QString &resource)
{
    const QString suffix = QFileInfo(resource).suffix().toLower();
    if (suffix == QLatin1String("html"))
        return QByteArrayLiteral("text/html");
    if (suffix == QLatin1String("js"))
        return QByteArrayLiteral("text/javascript");
    if (suffix == QLatin1String("css"))
        return QByteArrayLiteral("text/css");
    if (suffix == QLatin1String("svg"))
        return QByteArrayLiteral("image/svg+xml");
    return {};
}

QMultiMap<QByteArray, QByteArray> responseHeaders()
{
    QMultiMap<QByteArray, QByteArray> headers;
    // default-src does not cover these directives, so each is closed too.
    headers.insert(QByteArrayLiteral("Content-Security-Policy"),
                   QByteArrayLiteral("default-src dash:; connect-src 'none'; object-src 'none'; base-uri 'none'; "
                                     "form-action 'none'; frame-ancestors 'none'"));
    headers.insert(QByteArrayLiteral("X-Content-Type-Options"), QByteArrayLiteral("nosniff"));
    headers.insert(QByteArrayLiteral("Referrer-Policy"), QByteArrayLiteral("no-referrer"));
    // A panel changed in a new build loads fresh.
    headers.insert(QByteArrayLiteral("Cache-Control"), QByteArrayLiteral("no-store"));
    return headers;
}

} // namespace dash

DashSchemeHandler::DashSchemeHandler(QObject *parent)
    : QWebEngineUrlSchemeHandler(parent)
{
}

void DashSchemeHandler::requestStarted(QWebEngineUrlRequestJob *job)
{
    if (job->requestMethod() != QByteArrayLiteral("GET")) {
        job->fail(QWebEngineUrlRequestJob::RequestDenied);
        return;
    }
    const std::optional<QString> resource = dash::resourceFor(job->requestUrl());
    const QByteArray contentType = resource ? dash::contentTypeFor(*resource) : QByteArray();
    if (!resource || contentType.isEmpty()) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    // The job owns the file, which must stay open while the job reads it.
    auto *file = new QFile(*resource, job);
    if (!file->open(QIODevice::ReadOnly)) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    job->setAdditionalResponseHeaders(dash::responseHeaders());
    job->reply(contentType, file);
}

} // namespace mirante
