#include "web/PageArea.h"

#include "web/PageKeys.h"
#include "web/WebPage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedLayout>
#include <QVBoxLayout>
#include <QWebEngineLoadingInfo>
#include <QWebEngineView>
#include <QtDebug>

#include <deque>

namespace mirante {

namespace {

// Chromium's code for a load that was stopped, for example by a new load or
// by a download. It is not a failure to show.
constexpr int kAbortedErrorCode = -3;

// Whether a failed load shows the failure message instead of the page.
// Chromium's network errors, such as a host that does not resolve, have
// negative codes. A positive code is an HTTP status, such as 404: the server
// sent a page, which Chrome shows, so the panel shows it too.
bool showsFailure(QWebEngineLoadingInfo::ErrorDomain domain, int code)
{
    if (domain == QWebEngineLoadingInfo::HttpStatusCodeDomain)
        return false;
    return code < 0 && code != kAbortedErrorCode;
}

QString permissionQuestion(const QWebEnginePermission &permission)
{
    const QUrl origin = permission.origin();
    const QString site = origin.host().isEmpty() ? origin.toDisplayString() : origin.host();
    if (permission.permissionType() == QWebEnginePermission::PermissionType::Notifications)
        return PageArea::tr("%1 wants to show notifications.").arg(site);
    return PageArea::tr("%1 wants to read and write the clipboard.").arg(site);
}

} // namespace

struct PageArea::Private
{
    PageArea *q = nullptr;
    WebPage *page = nullptr;
    QWebEngineView *view = nullptr;
    PageKeys *keys = nullptr;

    // The permission bar asks about the first waiting permission.
    QFrame *permissionBar = nullptr;
    QLabel *permissionText = nullptr;
    std::deque<QWebEnginePermission> waiting;

    QStackedLayout *stack = nullptr;
    QWidget *failure = nullptr;
    QLabel *failureText = nullptr;
    QUrl failedUrl;

    void buildPermissionBar()
    {
        permissionBar = new QFrame(q);
        permissionBar->setObjectName(QStringLiteral("permissionBar"));
        permissionBar->setFrameShape(QFrame::StyledPanel);
        permissionBar->setAutoFillBackground(true);
        permissionBar->setBackgroundRole(QPalette::AlternateBase);

        permissionText = new QLabel(permissionBar);
        permissionText->setObjectName(QStringLiteral("permissionText"));
        permissionText->setWordWrap(true);

        auto *allow = new QPushButton(tr("Allow"), permissionBar);
        allow->setObjectName(QStringLiteral("allowPermission"));
        QObject::connect(allow, &QPushButton::clicked, q, [this] { answer(true); });

        auto *deny = new QPushButton(tr("Deny"), permissionBar);
        deny->setObjectName(QStringLiteral("denyPermission"));
        QObject::connect(deny, &QPushButton::clicked, q, [this] { answer(false); });

        auto *layout = new QHBoxLayout(permissionBar);
        layout->setContentsMargins(6, 2, 6, 2);
        layout->addWidget(permissionText, 1);
        layout->addWidget(allow);
        layout->addWidget(deny);
        permissionBar->hide();
    }

    void buildFailure()
    {
        failure = new QWidget(q);
        failure->setObjectName(QStringLiteral("loadFailure"));

        failureText = new QLabel(failure);
        failureText->setObjectName(QStringLiteral("failureText"));
        failureText->setAlignment(Qt::AlignCenter);
        failureText->setWordWrap(true);
        failureText->setTextInteractionFlags(Qt::TextSelectableByMouse);

        auto *reloadButton = new QPushButton(tr("Reload"), failure);
        reloadButton->setObjectName(QStringLiteral("failureReload"));
        QObject::connect(reloadButton, &QPushButton::clicked, q, [this] { q->reload(); });

        auto *layout = new QVBoxLayout(failure);
        layout->addStretch(1);
        layout->addWidget(failureText);
        layout->addWidget(reloadButton, 0, Qt::AlignHCenter);
        layout->addStretch(1);
    }

    void ask(const QWebEnginePermission &permission)
    {
        waiting.push_back(permission);
        showNext();
    }

    // Shows the first waiting permission that is still valid, or hides the
    // bar. A permission becomes invalid when its page goes away.
    void showNext()
    {
        while (!waiting.empty() && !waiting.front().isValid())
            waiting.pop_front();
        if (waiting.empty()) {
            permissionBar->hide();
            return;
        }
        permissionText->setText(permissionQuestion(waiting.front()));
        permissionBar->show();
    }

    // The profile keeps the answer, so the same site is not asked again.
    void answer(bool allow)
    {
        if (waiting.empty())
            return;
        QWebEnginePermission permission = waiting.front();
        waiting.pop_front();
        if (permission.isValid()) {
            if (allow)
                permission.grant();
            else
                permission.deny();
        }
        showNext();
    }

    void loadingChanged(const QWebEngineLoadingInfo &info)
    {
        switch (info.status()) {
        case QWebEngineLoadingInfo::LoadStartedStatus:
        case QWebEngineLoadingInfo::LoadSucceededStatus:
            stack->setCurrentWidget(view);
            break;
        case QWebEngineLoadingInfo::LoadFailedStatus: {
            qInfo().noquote() << "Load failed:" << info.url().toDisplayString() << "domain" << info.errorDomain()
                              << "code" << info.errorCode() << info.errorString();
            if (!showsFailure(info.errorDomain(), info.errorCode()))
                break;
            failedUrl = info.url();
            const QString reason = info.errorString().isEmpty()
                ? tr("Error %1").arg(info.errorCode())
                : info.errorString();
            failureText->setText(tr("Could not load %1\n%2").arg(info.url().toDisplayString(), reason));
            stack->setCurrentWidget(failure);
            break;
        }
        case QWebEngineLoadingInfo::LoadStoppedStatus:
            break;
        }
    }
};

PageArea::PageArea(QWebEngineProfile *profile, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;

    d->view = new QWebEngineView(this);
    // The page is the view's child, so it goes before its profile does.
    d->page = new WebPage(profile, d->view);
    d->view->setPage(d->page);
    d->keys = PageKeys::install(d->view);

    d->buildPermissionBar();
    d->buildFailure();

    d->stack = new QStackedLayout;
    d->stack->addWidget(d->view);
    d->stack->addWidget(d->failure);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(d->permissionBar);
    layout->addLayout(d->stack, 1);

    connect(d->page, &WebPage::permissionToAsk, this,
            [this](const QWebEnginePermission &permission) { d->ask(permission); });
    connect(d->page, &QWebEnginePage::loadingChanged, this,
            [this](const QWebEngineLoadingInfo &info) { d->loadingChanged(info); });
    // A page whose process ended, such as after a crash, would stay blank.
    connect(d->page, &QWebEnginePage::renderProcessTerminated, this,
            [this](QWebEnginePage::RenderProcessTerminationStatus status, int exitCode) {
                if (status == QWebEnginePage::NormalTerminationStatus)
                    return;
                qWarning().noquote() << "Page process ended:" << d->page->url().toDisplayString() << "status"
                                     << status << "exit code" << exitCode;
                d->failedUrl = d->page->url();
                d->failureText->setText(tr("The page %1 stopped.").arg(d->failedUrl.toDisplayString()));
                d->stack->setCurrentWidget(d->failure);
            });
}

PageArea::~PageArea() = default;

WebPage *PageArea::page() const { return d->page; }

QWebEngineView *PageArea::view() const { return d->view; }

void PageArea::reload()
{
    if (d->stack->currentWidget() == d->failure && d->failedUrl.isValid())
        d->page->load(d->failedUrl);
    else
        d->page->triggerAction(QWebEnginePage::Reload);
}

void PageArea::setPageKeys(std::function<QList<QKeySequence>()> keys) { d->keys->setKeys(std::move(keys)); }

} // namespace mirante
