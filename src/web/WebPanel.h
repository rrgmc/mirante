#pragma once

#include <QKeySequence>
#include <QList>
#include <QWebEnginePage>
#include <QWidget>

#include <functional>
#include <memory>

class QUrl;
class QWebEngineProfile;
class QWebEngineView;

namespace mirante {

// A web page with the behaviour of docs/web-views.md#page-behaviour: login
// popups in a dialog, other new windows in the external browser, permissions
// asked once per host, downloads after asking, and a toolbar.
// Implemented in WebPanel.cpp.
class WebPanel : public QWidget
{
    Q_OBJECT

public:
    // The profile must outlive the panel.
    WebPanel(QWebEngineProfile *profile, const QUrl &url, QWidget *parent = nullptr);
    ~WebPanel() override;

    // Back, reload, the current address and "Open in browser", for the cell
    // header. The panel owns it; the cell header shows it.
    QWidget *toolbar() const;

    QWebEngineView *view() const;

    // The keys the page keeps instead of the app's shortcuts, such as an
    // item's pageKeys. See PageArea::setPageKeys().
    void setPageKeys(std::function<QList<QKeySequence>()> keys);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

// How a page's request for a new window is handled. Pure, for tests.
enum class NewWindow { LoginPopup, ExternalBrowser };
NewWindow classifyNewWindow(QWebEnginePage::WebWindowType type, bool userInitiated);

} // namespace mirante
