#pragma once

#include <QWebEnginePage>
#include <QWebEngineView>

class QUrl;
class QWebEngineProfile;

namespace mirante {

// The page of a built-in panel. It goes only to dash: addresses, makes no new
// windows and denies every permission, so no page but a panel ever holds the
// bridge. See docs/decisions/0094-built-in-panels-as-a-core-item-type.md.
// Implemented in DashView.cpp.
class DashPage : public QWebEnginePage
{
    Q_OBJECT

public:
    // profile is the app profile, which serves dash://. It must outlive the page.
    explicit DashPage(QWebEngineProfile *profile, QObject *parent = nullptr);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;
    QWebEnginePage *createWindow(WebWindowType type) override;
};

// A built-in panel: a DashPage with the bridge registered on its web channel
// as "dash". It has no toolbar and no context menu.
class DashView : public QWebEngineView
{
    Q_OBJECT

public:
    // bridge may be null, for a page without data. It must outlive the view.
    DashView(QWebEngineProfile *profile, const QUrl &url, QObject *bridge, QWidget *parent = nullptr);
};

} // namespace mirante
