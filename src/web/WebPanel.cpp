#include "web/WebPanel.h"

#include "web/AddressField.h"
#include "web/PageArea.h"
#include "web/WebPage.h"

#include <QDesktopServices>
#include <QFile>
#include <QHBoxLayout>
#include <QPointer>
#include <QStyle>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineHistory>
#include <QWebEngineView>

namespace mirante {

namespace {

// An icon of the app, from resources/icons, or the style's icon if the app's
// icons are not linked in, as in this library's tests. The layout library
// compiles resources/icons in.
QIcon appIcon(const QString &name, QStyle::StandardPixmap fallback, const QWidget *widget)
{
    const QString path = QStringLiteral(":/icons/%1.svg").arg(name);
    if (QFile::exists(path))
        return QIcon(path);
    return widget->style()->standardIcon(fallback, nullptr, widget);
}

QToolButton *toolButton(QWidget *parent, const QString &objectName, const QString &text, const QString &toolTip)
{
    auto *button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setText(text);
    button->setToolTip(toolTip);
    button->setAutoRaise(true);
    return button;
}

} // namespace

struct WebPanel::Private
{
    PageArea *area = nullptr;

    // The toolbar has no parent until the cell header takes it, and the panel
    // deletes it, wherever it is by then.
    QPointer<QWidget> toolbar;
    QToolButton *back = nullptr;
    AddressField *address = nullptr;

    void buildToolbar()
    {
        toolbar = new QWidget;
        toolbar->setObjectName(QStringLiteral("webToolbar"));

        back = toolButton(toolbar, QStringLiteral("backButton"), tr("Back"), tr("Back"));
        back->setIcon(appIcon(QStringLiteral("back"), QStyle::SP_ArrowBack, toolbar));
        back->setEnabled(false);

        QToolButton *reload = toolButton(toolbar, QStringLiteral("reloadButton"), tr("Reload"), tr("Reload"));
        reload->setIcon(appIcon(QStringLiteral("reload"), QStyle::SP_BrowserReload, toolbar));

        address = new AddressField(toolbar);
        address->setObjectName(QStringLiteral("address"));

        QToolButton *open = toolButton(toolbar, QStringLiteral("openInBrowserButton"), tr("Open in browser"),
                                       tr("Open this page in the external browser"));
        open->setIcon(appIcon(QStringLiteral("open-in-browser"), QStyle::SP_DirLinkIcon, toolbar));

        auto *layout = new QHBoxLayout(toolbar);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);
        layout->addWidget(back);
        layout->addWidget(reload);
        layout->addWidget(address, 1);
        layout->addWidget(open);

        QWebEngineView *view = area->view();
        QObject::connect(back, &QToolButton::clicked, view, &QWebEngineView::back);
        QObject::connect(reload, &QToolButton::clicked, area, &PageArea::reload);
        QObject::connect(open, &QToolButton::clicked, view, [view] {
            const QUrl url = view->url();
            if (!url.isEmpty())
                QDesktopServices::openUrl(url);
        });
    }

    void updateToolbar()
    {
        QWebEngineView *view = area->view();
        back->setEnabled(view->history()->canGoBack());
        address->setAddress(view->url().toDisplayString());
    }
};

WebPanel::WebPanel(QWebEngineProfile *profile, const QUrl &url, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
    d->area = new PageArea(profile, this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(d->area);

    d->buildToolbar();

    // The history changes when a load starts and when it ends, and the
    // address can change without a load, as in a single-page app.
    QWebEngineView *view = d->area->view();
    connect(view, &QWebEngineView::urlChanged, this, [this] { d->updateToolbar(); });
    connect(view->page(), &QWebEnginePage::loadingChanged, this, [this] { d->updateToolbar(); });

    view->setUrl(url);
    d->updateToolbar();
}

WebPanel::~WebPanel()
{
    delete d->toolbar.data();
    // The page's signals use d, so it goes before d does.
    delete d->area;
}

QWidget *WebPanel::toolbar() const { return d->toolbar; }

QWebEngineView *WebPanel::view() const { return d->area->view(); }

void WebPanel::setPageKeys(std::function<QList<QKeySequence>()> keys) { d->area->setPageKeys(std::move(keys)); }

NewWindow classifyNewWindow(QWebEnginePage::WebWindowType type, bool userInitiated)
{
    // A window that names its size and features, as an OAuth sign-in does,
    // is a dialog. Its page needs the session of the page that opened it.
    Q_UNUSED(userInitiated)
    switch (type) {
    case QWebEnginePage::WebDialog:
        return NewWindow::LoginPopup;
    case QWebEnginePage::WebBrowserWindow:
    case QWebEnginePage::WebBrowserTab:
    case QWebEnginePage::WebBrowserBackgroundTab:
        break;
    }
    return NewWindow::ExternalBrowser;
}

} // namespace mirante
