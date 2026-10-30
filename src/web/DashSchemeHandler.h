#pragma once

#include <QByteArray>
#include <QMultiMap>
#include <QString>
#include <QWebEngineUrlSchemeHandler>

#include <optional>

class QUrl;

namespace mirante {

// The dash:// scheme of the built-in panels. See
// docs/web-views.md#built-in-panels and the M9 plan, part 1.
namespace dash {

// "dash".
QByteArray schemeName();

// Registers the scheme with Qt WebEngine. Call it before QApplication is made.
void registerScheme();

// True once registerScheme() was called in this process.
bool isRegistered();

// The resource that serves an address, such as ":/panels/task-summary/index.html"
// for dash://panels/task-summary/index.html, or nothing for an address the
// scheme does not serve: another host, a path that climbs out with "..", or
// a folder. dash://panels/common/qwebchannel.js is Qt's own qwebchannel.js.
std::optional<QString> resourceFor(const QUrl &url);

// The content type of a resource from its suffix, such as "text/html", or
// empty for a suffix the scheme does not serve.
QByteArray contentTypeFor(const QString &resource);

// The headers sent with every reply: the Content-Security-Policy and the
// others of the M9 plan.
QMultiMap<QByteArray, QByteArray> responseHeaders();

} // namespace dash

// Serves dash:// from the resources. Only the app profile installs it, so
// no other profile can load a panel. Implemented in DashSchemeHandler.cpp.
class DashSchemeHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT

public:
    explicit DashSchemeHandler(QObject *parent = nullptr);

    void requestStarted(QWebEngineUrlRequestJob *job) override;
};

} // namespace mirante
