#pragma once

#include <QKeySequence>
#include <QList>
#include <QWidget>

#include <functional>
#include <memory>

class QWebEngineProfile;
class QWebEngineView;

namespace mirante {

class WebPage;

// A WebPage in a view, with a slim bar above it that asks for the clipboard
// and notification permissions, and a message with a Reload button in its
// place when a load fails. WebPanel and LoginDialog show their page in one.
// See docs/web-views.md#page-behaviour. Implemented in PageArea.cpp.
class PageArea : public QWidget
{
    Q_OBJECT

public:
    // The profile must outlive the area.
    explicit PageArea(QWebEngineProfile *profile, QWidget *parent = nullptr);
    ~PageArea() override;

    WebPage *page() const;
    QWebEngineView *view() const;

    // Loads the page again. After a failed load it loads the address that
    // failed, which a plain reload may not know.
    void reload();

    // The keys the page keeps instead of the app's shortcuts, asked on each
    // key. See PageKeys.
    void setPageKeys(std::function<QList<QKeySequence>()> keys);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
