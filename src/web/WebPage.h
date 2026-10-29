#pragma once

#include <QWebEngineNewWindowRequest>
#include <QWebEnginePage>
#include <QWebEnginePermission>

class QWebEngineProfile;

namespace mirante {

// The page of a WebPanel or of a login dialog. It opens login popups in a
// LoginDialog and other new windows in the external browser, and denies the
// permissions that are never asked. See docs/web-views.md#page-behaviour.
// Implemented in WebPage.cpp.
class WebPage : public QWebEnginePage
{
    Q_OBJECT

public:
    // The profile must outlive the page.
    explicit WebPage(QWebEngineProfile *profile, QObject *parent = nullptr);

signals:
    // A permission that the user decides on: the clipboard or notifications.
    // The PageArea showing the page asks in its permission bar.
    void permissionToAsk(QWebEnginePermission permission);

private:
    void handleNewWindow(QWebEngineNewWindowRequest &request);
    void handlePermission(QWebEnginePermission permission);
};

// Whether a permission is asked or denied without asking. Pure, for tests.
enum class PermissionHandling { Ask, Deny };
PermissionHandling permissionHandling(QWebEnginePermission::PermissionType type);

// The window type that matches where a new window request wants to open.
QWebEnginePage::WebWindowType windowTypeOf(QWebEngineNewWindowRequest::DestinationType destination);

} // namespace mirante
