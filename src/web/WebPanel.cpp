#include "web/WebPanel.h"

#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineView>

namespace mirante {

// Stub from M4 T0. T2 implements it.
struct WebPanel::Private
{
    QWebEngineView *view = nullptr;
    QWidget *toolbar = nullptr;
};

WebPanel::WebPanel(QWebEngineProfile *profile, const QUrl &url, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
    Q_UNUSED(profile)
    Q_UNUSED(url)
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    d->view = new QWebEngineView(this);
    layout->addWidget(d->view);
    d->toolbar = new QWidget(this);
}

WebPanel::~WebPanel() = default;

QWidget *WebPanel::toolbar() const { return d->toolbar; }

QWebEngineView *WebPanel::view() const { return d->view; }

NewWindow classifyNewWindow(QWebEnginePage::WebWindowType type, bool userInitiated)
{
    Q_UNUSED(type)
    Q_UNUSED(userInitiated)
    return NewWindow::ExternalBrowser;
}

} // namespace mirante
