#include "web/WebPage.h"

#include "web/LoginDialog.h"
#include "web/WebPanel.h"

#include <QDesktopServices>
#include <QTimer>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

namespace mirante {

namespace {

// How long a page that waits for the address of an external window lives.
constexpr int kCaptureLifetimeMs = 30 * 1000;

bool isBlank(const QUrl &url)
{
    return url.isEmpty() || url == QUrl(QStringLiteral("about:blank"));
}

// A hidden page for a new window that goes to the external browser but has no
// address yet, as with window.open() followed by setting its location. The
// first real address it is sent to opens in the external browser, and the
// page goes away.
class CapturePage : public QWebEnginePage
{
public:
    CapturePage(QWebEngineProfile *profile, QObject *parent)
        : QWebEnginePage(profile, parent)
    {
        connect(this, &QWebEnginePage::windowCloseRequested, this, &QObject::deleteLater);
        // A page that never navigates, for example one the opener writes into,
        // does not stay for ever.
        QTimer::singleShot(kCaptureLifetimeMs, this, &QObject::deleteLater);
    }

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override
    {
        Q_UNUSED(type)
        if (!isMainFrame || isBlank(url))
            return true;
        if (!m_opened) {
            m_opened = true;
            QDesktopServices::openUrl(url);
            deleteLater();
        }
        return false;
    }

private:
    bool m_opened = false;
};

} // namespace

WebPage::WebPage(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent)
{
    // A failed load shows the PageArea's own message, not Chromium's page.
    settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, false);

    connect(this, &QWebEnginePage::newWindowRequested, this, &WebPage::handleNewWindow);
    connect(this, &QWebEnginePage::permissionRequested, this, &WebPage::handlePermission);
}

void WebPage::handleNewWindow(QWebEngineNewWindowRequest &request)
{
    const NewWindow kind = classifyNewWindow(windowTypeOf(request.destination()), request.isUserInitiated());

    if (kind == NewWindow::LoginPopup) {
        // The dialog belongs to the window that shows this page, so it stays
        // on top of it and closes with it.
        QWidget *owner = nullptr;
        if (QWebEngineView *view = QWebEngineView::forPage(this))
            owner = view->window();
        auto *dialog = new LoginDialog(profile(), request.requestedGeometry().size(), owner);
        request.openIn(dialog->page());
        dialog->show();
        return;
    }

    const QUrl url = request.requestedUrl();
    if (!isBlank(url)) {
        // The request is left unhandled, so no page is made for it.
        QDesktopServices::openUrl(url);
        return;
    }
    request.openIn(new CapturePage(profile(), this));
}

void WebPage::handlePermission(QWebEnginePermission permission)
{
    if (permissionHandling(permission.permissionType()) == PermissionHandling::Ask)
        emit permissionToAsk(permission);
    else
        permission.deny();
}

PermissionHandling permissionHandling(QWebEnginePermission::PermissionType type)
{
    switch (type) {
    case QWebEnginePermission::PermissionType::ClipboardReadWrite:
    case QWebEnginePermission::PermissionType::Notifications:
        return PermissionHandling::Ask;
    case QWebEnginePermission::PermissionType::MediaAudioCapture:
    case QWebEnginePermission::PermissionType::MediaVideoCapture:
    case QWebEnginePermission::PermissionType::MediaAudioVideoCapture:
    case QWebEnginePermission::PermissionType::DesktopVideoCapture:
    case QWebEnginePermission::PermissionType::DesktopAudioVideoCapture:
    case QWebEnginePermission::PermissionType::Geolocation:
    case QWebEnginePermission::PermissionType::MouseLock:
    case QWebEnginePermission::PermissionType::LocalFontsAccess:
    case QWebEnginePermission::PermissionType::Unsupported:
        break;
    }
    return PermissionHandling::Deny;
}

QWebEnginePage::WebWindowType windowTypeOf(QWebEngineNewWindowRequest::DestinationType destination)
{
    switch (destination) {
    case QWebEngineNewWindowRequest::InNewWindow:
        return QWebEnginePage::WebBrowserWindow;
    case QWebEngineNewWindowRequest::InNewTab:
        return QWebEnginePage::WebBrowserTab;
    case QWebEngineNewWindowRequest::InNewDialog:
        return QWebEnginePage::WebDialog;
    case QWebEngineNewWindowRequest::InNewBackgroundTab:
        return QWebEnginePage::WebBrowserBackgroundTab;
    }
    return QWebEnginePage::WebBrowserWindow;
}

} // namespace mirante
