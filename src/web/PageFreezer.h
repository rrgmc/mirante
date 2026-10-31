#pragma once

#include <QList>
#include <QObject>
#include <QPointer>

class QWebEnginePage;
class QWidget;

namespace mirante {

// Freezes the web pages of windows hidden to the tray, so that they run no
// scripts and load nothing until the windows show again. A page is frozen
// only when Chromium recommends it, so a page that plays sound or has its
// developer tools open stays active until that ends. Pages are never
// discarded, so none loses what the user typed. See
// docs/ui.md#tray-and-notifications and decision 0108.
class PageFreezer : public QObject
{
    Q_OBJECT

public:
    explicit PageFreezer(QObject *parent = nullptr);
    ~PageFreezer() override;

    // Freezes the pages of the web views in these windows, which must be
    // hidden. A page that must stay active now is frozen once Chromium allows
    // it. Pages from an earlier call stay frozen.
    void freeze(const QList<QWidget *> &windows);
    // Makes every page that freeze() saw active again.
    void thaw();

private:
    void follow(QWebEnginePage *page);

    QList<QPointer<QWebEnginePage>> m_pages;
    // Holds the connections to the pages, so that thaw() ends them all.
    QObject *m_watch = nullptr;
};

} // namespace mirante
