#include "web/DashView.h"

#include "web/DashSchemeHandler.h"

#include <QUrl>
#include <QWebChannel>
#include <QWebEnginePermission>

namespace mirante {

DashPage::DashPage(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent)
{
    // A panel needs no camera, location or notifications.
    connect(this, &QWebEnginePage::permissionRequested, this,
            [](QWebEnginePermission permission) { permission.deny(); });
}

bool DashPage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
{
    Q_UNUSED(type)
    Q_UNUSED(isMainFrame)
    return url.scheme() == QLatin1String(dash::schemeName());
}

QWebEnginePage *DashPage::createWindow(WebWindowType type)
{
    Q_UNUSED(type)
    return nullptr;
}

DashView::DashView(QWebEngineProfile *profile, const QUrl &url, QObject *bridge, QWidget *parent)
    : QWebEngineView(parent)
{
    setContextMenuPolicy(Qt::NoContextMenu);
    auto *page = new DashPage(profile, this);
    if (bridge) {
        auto *channel = new QWebChannel(page);
        channel->registerObject(QStringLiteral("dash"), bridge);
        page->setWebChannel(channel);
    }
    setPage(page);
    load(url);
}

} // namespace mirante
