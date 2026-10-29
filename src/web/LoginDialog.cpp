#include "web/LoginDialog.h"

#include "web/PageArea.h"
#include "web/WebPage.h"

#include <QGuiApplication>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>

namespace mirante {

namespace {

constexpr int kMinimumWidth = 500;
constexpr int kMinimumHeight = 600;

} // namespace

struct LoginDialog::Private
{
    PageArea *area = nullptr;
};

LoginDialog::LoginDialog(QWebEngineProfile *profile, const QSize &requested, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Sign in"));

    d->area = new PageArea(profile, this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(d->area);

    WebPage *page = d->area->page();
    connect(page, &QWebEnginePage::windowCloseRequested, this, &QDialog::close);
    connect(page, &QWebEnginePage::titleChanged, this, [this](const QString &title) {
        setWindowTitle(title.isEmpty() ? tr("Sign in") : title);
    });

    QScreen *screen = parent ? parent->screen() : QGuiApplication::primaryScreen();
    const QSize available = screen ? screen->availableSize() : QSize();
    resize(loginDialogSize(requested, available));
}

LoginDialog::~LoginDialog() = default;

WebPage *LoginDialog::page() const { return d->area->page(); }

QSize loginDialogSize(const QSize &requested, const QSize &available)
{
    QSize size(std::max(requested.width(), kMinimumWidth), std::max(requested.height(), kMinimumHeight));
    if (available.isValid())
        size = size.boundedTo(available);
    return size;
}

} // namespace mirante
