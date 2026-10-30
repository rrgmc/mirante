#include "items/Placement.h"

#include "api/Attention.h"
#include "api/StatusCache.h"
#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "core/Profiles.h"
#include "items/ItemContextImpl.h"
#include "items/ItemTypeRegistry.h"
#include "items/ViewPool.h"
#include "web/ProfileManager.h"

#include <mirante/ItemType.h>
#include <mirante/Status.h>

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

namespace mirante {

namespace {

// Placement.h declares no event handlers, so the placement watches its own
// show and hide events through this filter.
class VisibilityWatcher : public QObject
{
public:
    VisibilityWatcher(QWidget *widget, std::function<void(bool)> changed)
        : QObject(widget)
        , m_changed(std::move(changed))
    {
        widget->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == parent() && (event->type() == QEvent::Show || event->type() == QEvent::Hide))
            m_changed(event->type() == QEvent::Show);
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void(bool)> m_changed;
};

// How long a cell must be visible in the active window before its item's
// clearsOnView reasons count as seen. See decision 0061.
constexpr int seenAfterMs = 2000;
// How often the age of old data is written again.
constexpr int ageEveryMs = 60 * 1000;

// The age of data fetched at time, such as "12 min ago".
QString ageText(const QDateTime &time)
{
    const qint64 minutes = time.secsTo(QDateTime::currentDateTimeUtc()) / 60;
    if (minutes < 1)
        return Placement::tr("just now");
    if (minutes < 60)
        return Placement::tr("%1 min ago").arg(minutes);
    if (minutes < 24 * 60)
        return Placement::tr("%1 h ago").arg(minutes / 60);
    return Placement::tr("%n day(s) ago", nullptr, int(minutes / (24 * 60)));
}

QString reasonTexts(const QList<AttentionReason> &reasons)
{
    QStringList texts;
    for (const AttentionReason &reason : reasons)
        texts << reason.text;
    return texts.join(QLatin1Char('\n'));
}

} // namespace

struct Placement::Private
{
    Placement *q = nullptr;
    PlacementServices services;
    QString taskId;
    QString itemId;
    ItemContextImpl context;

    QVBoxLayout *layout = nullptr;
    QPointer<QWidget> content;          // what the layout shows: the view or a placeholder
    QPointer<QWidget> view;
    QPointer<QWidget> header;
    QMetaObject::Connection headerDestroyed;
    // The page of showPage(), shown in place of the view, which stays alive
    // and hidden. See decision 0069.
    QPointer<QWidget> page;
    QPointer<QWidget> pageHeader;       // the back button and the page's toolbar
    QPointer<QWidget> viewHeader;       // the view's header widget, while a page is shown
    QString pageProfile;                // the profile the page was made in
    // Widgets deleted later, so that a view can change the model from its own
    // slot. The destructor deletes those still waiting, while the context
    // they may use still exists.
    std::vector<QPointer<QWidget>> dying;

    // Set while the shown content was made for madeType, madeConfig and
    // madeProfile: the view, or the placeholder of a type without one.
    bool made = false;
    QString madeType;
    QJsonObject madeConfig;
    QString madeProfile;                // the profile id of a web view; empty for a type that is not web
    QJsonObject madeProfileData;        // the profile's module settings, for a type that uses them
    QJsonObject madeFolders;            // the local paths of the project's folders, for a native view
    QLabel *typePlaceholder = nullptr;  // the placeholder of a type without a view, if shown
    bool gone = false;                  // the item or its task no longer exists
    QString waitingFor;                 // the profile being cleared, while the view waits for it
    bool inPool = false;
    bool heavyView = false;             // the view is a web view, which the pool may unload
    QObject *watcher = nullptr;

    // Status. See the M5 plan, part 8.
    CellStatus cellStatus;
    bool hadStatus = false;             // hasStatus() when cellStatus was built
    bool statusVisible = false;         // told the status cache this placement is visible
    QTimer seenTimer;                   // runs while clearsOnView reasons wait to be seen
    QTimer ageTimer;                    // runs while the cell shows the age of old data

    Private(Placement *placement, const PlacementServices &s, const QString &task, const QString &item)
        : q(placement)
        , services(s)
        , taskId(task)
        , itemId(item)
        , context(s, task, item)
    {
    }

    const Item *item() const { return services.model ? services.model->item(taskId, itemId) : nullptr; }

    // The profile a web view of the item would use now. Empty for a type
    // whose views are not web views, so a change of profile leaves it alone.
    QString profileOf(const Item &found) const
    {
        if (!services.types || !services.types->contains(found.type)
            || services.types->type(found.type).rendering() != Rendering::Web)
            return {};
        return mirante::profiles::effective(*services.model, taskId, itemId);
    }

    // The module settings of the profile the item uses, for
    // a type that uses a profile. A change of them makes the view again,
    // since the view may have read them. Empty for other types.
    QJsonObject profileDataOf(const Item &found) const
    {
        if (!services.types || !services.types->usesProfile(found.type))
            return {};
        const Profile *profile =
            services.model->profile(mirante::profiles::effective(*services.model, taskId, itemId));
        if (!profile)
            return {};
        QJsonObject json = profile->toJson();
        json.remove(QStringLiteral("name"));
        return json;
    }

    // The local paths of the project's folders on this device, by folder id,
    // for a type whose views are not web views. A native view may show them,
    // such as the Folder item, so a new mapping makes the view again. Empty
    // for a web view, which would lose its page.
    QJsonObject foldersOf(const Item &found) const
    {
        if (!services.settings || !services.types
            || services.types->type(found.type).rendering() == Rendering::Web)
            return {};
        const Task *task = services.model->task(taskId);
        const Project *project = task ? services.model->project(task->projectId) : nullptr;
        if (!project)
            return {};
        QJsonObject paths;
        for (const Folder &folder : project->folders)
            paths.insert(folder.id, services.settings->folderPath(folder.id));
        return paths;
    }

    // Whether the item's configuration differs from the one the view was made
    // with only in keys of the type's viewKeys(). See decision 0081.
    bool onlyViewKeysChanged(const Item &found) const
    {
        if (!services.types || found.type != madeType)
            return false;
        const QStringList viewKeys = services.types->type(found.type).viewKeys();
        if (viewKeys.isEmpty())
            return false;
        QJsonObject now = found.config;
        QJsonObject before = madeConfig;
        for (const QString &key : viewKeys) {
            now.remove(key);
            before.remove(key);
        }
        return now == before;
    }

    void retire(QWidget *widget)
    {
        if (!widget)
            return;
        layout->removeWidget(widget);
        widget->hide();
        dying.erase(std::remove(dying.begin(), dying.end(), nullptr), dying.end());
        dying.emplace_back(widget);
        widget->deleteLater();
    }

    // Shows widget in place of the current content. Null shows nothing.
    void setContent(QWidget *widget)
    {
        typePlaceholder = nullptr;
        if (content && content != widget)
            retire(content);
        content = widget;
        if (widget) {
            layout->addWidget(widget);
            widget->show();
        }
    }

    void setHeader(QWidget *widget)
    {
        if (header == widget)
            return;
        QObject::disconnect(headerDestroyed);
        header = widget;
        // A view may delete its web page itself, and the toolbar with it.
        if (widget) {
            headerDestroyed = QObject::connect(widget, &QObject::destroyed, q, [this] {
                header = nullptr;
                emit q->headerWidgetChanged(nullptr);
            });
        }
        emit q->headerWidgetChanged(widget);
    }

    // Web views and pages are heavy, so the pool may unload them while hidden.
    // A placement has one entry: the page while one is shown, else a web view.
    void updatePool()
    {
        if (!services.pool)
            return;
        if (!page && !heavyView) {
            if (inPool) {
                inPool = false;
                services.pool->remove(q);
            }
            return;
        }
        inPool = true;
        services.pool->add(q, [this] {
            // The pool has already forgotten this placement.
            inPool = false;
            if (page)
                closePage();
            else
                unload();
        });
        if (!q->isVisible())
            services.pool->setVisible(q, false);
    }

    // Deletes the page and its header widget later, as for a view. The page
    // goes first, because its signals use the toolbar in the header widget.
    void discardPage()
    {
        retire(page);
        retire(pageHeader);
        page = nullptr;
        pageHeader = nullptr;
    }

    // The header widget of showPage(): the way back, then the page's toolbar.
    // It has no parent until the cell header takes it, as for a toolbar.
    QWidget *createPageHeader(QWidget *toolbar)
    {
        auto *bar = new QWidget;
        bar->setObjectName(QStringLiteral("pageHeader"));
        auto *row = new QHBoxLayout(bar);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(2);
        auto *back = new QToolButton(bar);
        back->setObjectName(QStringLiteral("backToSummary"));
        back->setText(Placement::tr("Back to summary"));
        QObject::connect(back, &QToolButton::clicked, q, [this] { closePage(); });
        row->addWidget(back);
        if (toolbar)
            row->addWidget(toolbar, 1);
        else
            row->addStretch(1);
        return bar;
    }

    // Shows a page in place of the view. A further page replaces the one
    // shown, and a placement without a live view has nothing to come back to.
    void openPage(const QUrl &url)
    {
        if (!view || !item())
            return;
        if (page)
            discardPage();
        else
            viewHeader = header;
        QWidget *toolbar = nullptr;
        page = context.createPage(url, q, toolbar);
        page->setObjectName(QStringLiteral("itemPage"));
        pageProfile = mirante::profiles::effective(*services.model, taskId, itemId);
        pageHeader = createPageHeader(toolbar);
        view->hide();
        layout->addWidget(page);
        page->show();
        setHeader(pageHeader);
        updatePool();
    }

    // Drops the page and shows the view again, as it was.
    void closePage()
    {
        if (!page)
            return;
        setHeader(viewHeader);
        viewHeader = nullptr;
        discardPage();
        if (view)
            view->show();
        updatePool();
    }

    // The view gave a header widget. While a page is shown, it waits.
    void setViewHeader(QWidget *widget)
    {
        if (page)
            viewHeader = widget;
        else
            setHeader(widget);
    }

    // Removes the view, its page, its header widget and its pool entry, and
    // shows replacement instead.
    void dropView(QWidget *replacement)
    {
        if (inPool) {
            inPool = false;
            if (services.pool)
                services.pool->remove(q);
        }
        heavyView = false;
        setHeader(nullptr);
        viewHeader = nullptr;
        discardPage();
        if (view) {
            if (content == view)
                content = nullptr;
            retire(view);
            view = nullptr;
        }
        made = false;
        setContent(replacement);
    }

    void makeView()
    {
        // A profile being cleared gets its views back once it is done.
        if (!waitingFor.isEmpty())
            return;
        const Item *found = item();
        if (!found) {
            gone = true;
            dropView(createMessageLabel(Placement::tr("Removed"), q));
            return;
        }
        dropView(nullptr);
        gone = false;
        made = true;
        madeType = found->type;
        madeConfig = found->config;
        madeProfile = profileOf(*found);
        madeProfileData = profileDataOf(*found);
        madeFolders = foldersOf(*found);

        const ItemType *type = services.types ? &services.types->type(found->type) : nullptr;
        QWidget *created = type ? type->createView(context, q) : nullptr;
        if (!created) {
            setHeader(nullptr);
            auto *label = createMessageLabel(itemPlaceholderText(type, found->name, itemId), q);
            setContent(label);
            typePlaceholder = label;
            return;
        }
        view = created;
        setContent(created);

        heavyView = type->rendering() == Rendering::Web;
        updatePool();
    }

    // The item's type, if it reports status and there is a status cache.
    const ItemType *statusType() const
    {
        const Item *found = item();
        if (!services.status || !services.types || !found)
            return nullptr;
        const ItemType &type = services.types->type(found->type);
        return type.hasStatus() ? &type : nullptr;
    }

    // The item's reasons that count now.
    QList<AttentionReason> countingReasons(const ItemStatus &status) const
    {
        if (services.attention)
            return services.attention->reasons(itemId);
        // Without Attention nothing records what was seen, so only states count.
        QList<AttentionReason> reasons;
        for (const AttentionReason &reason : status.attention) {
            if (!reason.clearsOnView)
                reasons << reason;
        }
        return reasons;
    }

    CellStatus buildStatus() const
    {
        CellStatus built;
        if (!statusType())
            return built;
        const ItemStatus status = services.status->status(itemId);
        using State = ItemStatus::State;
        const auto problem = [&](const QString &text, bool fixable) {
            built.kind = CellStatus::Kind::Problem;
            built.text = text;
            built.canFix = fixable && bool(services.openServerSettings);
            const QString profileId = profiles::effective(*services.model, taskId, itemId);
            built.tooltip = Placement::tr("%1\nProfile: %2")
                                .arg(status.message, profiles::nameOf(*services.model, profileId));
        };
        switch (status.state) {
        case State::None:
            break;
        case State::NoToken:
            problem(Placement::tr("Token not set"), true);
            break;
        case State::Refused:
            problem(Placement::tr("Token refused"), true);
            break;
        case State::NoServer:
            problem(Placement::tr("No server set"), true);
            break;
        case State::NotFound:
            problem(Placement::tr("Not found"), false);
            break;
        case State::Error:
            // Older data shows its age. Without any, the error is a problem.
            if (status.updatedAt.isValid()) {
                built.kind = CellStatus::Kind::Stale;
                built.text = ageText(status.updatedAt);
                built.tooltip = Placement::tr("%1\nThe data is from %2.").arg(status.message, built.text);
            } else {
                problem(Placement::tr("Error"), false);
            }
            break;
        case State::Ok: {
            const QList<AttentionReason> reasons = countingReasons(status);
            if (!reasons.isEmpty()) {
                built.kind = CellStatus::Kind::Attention;
                built.count = int(reasons.size());
                built.tooltip = reasonTexts(reasons);
            }
            break;
        }
        }
        return built;
    }

    // Builds the cell status again, and tells the frame if it changed.
    void updateStatus()
    {
        const CellStatus built = buildStatus();
        if (built.kind != CellStatus::Kind::Stale)
            ageTimer.stop();
        else if (!ageTimer.isActive())
            ageTimer.start();
        // A new type may add or remove status, and with it the refresh
        // button, so that change is told too.
        const bool has = statusType() != nullptr;
        if (built != cellStatus || has != hadStatus) {
            cellStatus = built;
            hadStatus = has;
            emit q->cellStatusChanged(cellStatus);
        }
        updateSeenTimer();
    }

    bool seenNow() const
    {
        const QWidget *window = q->window();
        return statusVisible && window && window->isActiveWindow();
    }

    // Starts the two seconds after which the item's clearsOnView reasons
    // count as seen, while some count and the cell is visible in the active
    // window. Stops them otherwise.
    void updateSeenTimer()
    {
        bool waiting = false;
        if (services.attention && statusType() && seenNow()) {
            for (const AttentionReason &reason : services.attention->reasons(itemId))
                waiting = waiting || reason.clearsOnView;
        }
        if (!waiting)
            seenTimer.stop();
        else if (!seenTimer.isActive())
            seenTimer.start();
    }

    void onSeenTimer()
    {
        if (services.attention && seenNow())
            services.attention->markSeen(itemId);
    }

    void onVisibilityChanged(bool shown)
    {
        // The status cache counts visible placements, so it hears each
        // change once.
        if (services.status && shown != statusVisible) {
            statusVisible = shown;
            services.status->setVisible(itemId, shown);
        }
        updateSeenTimer();

        // The first show makes the view, and so does a show after unloading
        // or after a change made while hidden.
        // A folder may also have been mapped while the placement was hidden,
        // which the model does not report.
        if (shown && !made && !gone) {
            makeView();
        } else if (shown && made && !gone) {
            if (const Item *found = item(); found && foldersOf(*found) != madeFolders)
                onModelChanged();
        }
        if (inPool && services.pool)
            services.pool->setVisible(q, shown);
    }

    // The user clears the data of a profile. A view in it goes at once, so
    // the profile can be released, and comes back when the clearing is done.
    void onProfileClearing(const QString &profileId)
    {
        // A page in that profile goes too, even when its view uses no profile.
        if (page && pageProfile == profileId)
            closePage();
        const Item *found = item();
        const QString profile = made ? madeProfile : (found ? profileOf(*found) : QString());
        if (profile.isEmpty() || profile != profileId)
            return;
        waitingFor = profileId;
        dropView(createMessageLabel(Placement::tr("Clearing this profile's data…"), q));
    }

    void onProfileCleared(const QString &profileId)
    {
        if (waitingFor.isEmpty() || waitingFor != profileId)
            return;
        waitingFor.clear();
        if (q->isVisible())
            makeView();
    }

    // Called by the pool. The view goes until the placement is shown again.
    void unload()
    {
        auto *placeholder = new QWidget(q);
        placeholder->setObjectName(QStringLiteral("reloadPlaceholder"));
        auto *placeholderLayout = new QVBoxLayout(placeholder);
        placeholderLayout->addStretch(1);
        placeholderLayout->addWidget(
            createMessageLabel(Placement::tr("This page was unloaded to save memory."), placeholder));
        auto *reload = new QPushButton(Placement::tr("Reload"), placeholder);
        placeholderLayout->addWidget(reload, 0, Qt::AlignHCenter);
        placeholderLayout->addStretch(1);
        QObject::connect(reload, &QPushButton::clicked, q, [this] { makeView(); });
        dropView(placeholder);
    }

    // The model changed. The view is made again when the item's type,
    // configuration or profile changed, the module settings of that profile,
    // or the local paths of the project's folders, and dropped when the item
    // is gone. A change to only the type's viewKeys() keeps the view and tells
    // it instead; see decision 0081. A hidden placement makes its new view
    // when it is shown.
    void onModelChanged()
    {
        const Item *found = item();
        if (!found) {
            if (!gone) {
                gone = true;
                dropView(createMessageLabel(Placement::tr("Removed"), q));
            }
            return;
        }
        if (gone) {
            // The item came back, for example after a reload of the files.
            gone = false;
            dropView(nullptr);
        } else if (!made) {
            return;
        } else if (found->type != madeType || profileOf(*found) != madeProfile
                   || profileDataOf(*found) != madeProfileData || foldersOf(*found) != madeFolders
                   || (found->config != madeConfig && !onlyViewKeysChanged(*found))) {
            dropView(nullptr);
        } else {
            // Only keys the view writes itself changed: it stays, and is told.
            if (found->config != madeConfig) {
                // A copy, since a receiver may change the model, and with it
                // madeConfig, while it is called.
                const QJsonObject config = found->config;
                madeConfig = config;
                context.notifyConfigChanged(config);
                found = item();
                if (!found)
                    return;
            }
            // A view that uses no profile stays, but its page must not outlive
            // the profile it was made in.
            if (page && mirante::profiles::effective(*services.model, taskId, itemId) != pageProfile)
                closePage();
            // A new name changes only the placeholder's text.
            if (typePlaceholder) {
                const ItemType *type = services.types ? &services.types->type(found->type) : nullptr;
                typePlaceholder->setText(itemPlaceholderText(type, found->name, itemId));
            }
            return;
        }
        if (q->isVisible())
            makeView();
    }
};

Placement::Placement(const PlacementServices &services, const QString &taskId, const QString &itemId, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>(this, services, taskId, itemId))
{
    d->layout = new QVBoxLayout(this);
    d->layout->setContentsMargins(0, 0, 0, 0);
    d->layout->setSpacing(0);

    d->context.onHeaderWidget = [this](QWidget *widget) { d->setViewHeader(widget); };
    d->context.onShowPage = [this](const QUrl &url) { d->openPage(url); };
    d->watcher = new VisibilityWatcher(this, [this](bool shown) { d->onVisibilityChanged(shown); });

    if (Model *model = services.model) {
        connect(model, &Model::taskChanged, this, [this](const QString &id) {
            if (id == d->taskId)
                d->onModelChanged();
        });
        connect(model, &Model::taskRemoved, this, [this](const QString &id) {
            if (id == d->taskId)
                d->onModelChanged();
        });
        connect(model, &Model::modelReset, this, [this] { d->onModelChanged(); });
        // The profile also depends on the group, the project's group and the
        // workspace's list of profiles.
        const auto profileMayChange = [this] {
            if (d->made && (!d->madeProfile.isEmpty() || !d->madeProfileData.isEmpty() || d->page))
                d->onModelChanged();
        };
        connect(model, &Model::groupChanged, this, profileMayChange);
        connect(model, &Model::projectChanged, this, profileMayChange);
        connect(model, &Model::profilesChanged, this, profileMayChange);
        // The project dialog maps folders as it changes the project.
        connect(model, &Model::projectChanged, this, [this](const QString &id) {
            const Task *task = d->services.model->task(d->taskId);
            if (d->made && task && task->projectId == id)
                d->onModelChanged();
        });
    }
    if (ProfileManager *profiles = services.profiles) {
        connect(profiles, &ProfileManager::profileClearing, this,
                [this](const QString &id) { d->onProfileClearing(id); });
        connect(profiles, &ProfileManager::profileCleared, this,
                [this](const QString &id) { d->onProfileCleared(id); });
    }

    // Status. A change of the item's type, or of its profile's name, changes
    // what the header shows too.
    d->seenTimer.setSingleShot(true);
    d->seenTimer.setInterval(seenAfterMs);
    connect(&d->seenTimer, &QTimer::timeout, this, [this] { d->onSeenTimer(); });
    d->ageTimer.setInterval(ageEveryMs);
    connect(&d->ageTimer, &QTimer::timeout, this, [this] { d->updateStatus(); });
    if (StatusCache *status = services.status) {
        status->source(itemId).subscribe(this, [this](const ItemStatus &) { d->updateStatus(); });
        if (Model *model = services.model) {
            connect(model, &Model::taskChanged, this, [this](const QString &id) {
                if (id == d->taskId)
                    d->updateStatus();
            });
            connect(model, &Model::modelReset, this, [this] { d->updateStatus(); });
            connect(model, &Model::profilesChanged, this, [this] { d->updateStatus(); });
        }
        if (Attention *attention = services.attention)
            connect(attention, &Attention::countsChanged, this, [this] { d->updateStatus(); });
        // The active window changed. Queued, so the widgets' active window
        // has followed the focus window.
        connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, [this] { d->updateSeenTimer(); },
                Qt::QueuedConnection);
        d->updateStatus();
    }
}

Placement::~Placement()
{
    // No more show or hide events reach d.
    delete d->watcher;
    if (d->statusVisible)
        d->services.status->setVisible(d->itemId, false);
    if (d->inPool && d->services.pool)
        d->services.pool->remove(this);
    d->inPool = false;
    QObject::disconnect(d->headerDestroyed);
    // The views go while the context they may use still exists. The page goes
    // first, because its signals use the toolbar in its header widget.
    delete d->page.data();
    delete d->pageHeader.data();
    delete d->view.data();
    for (const QPointer<QWidget> &widget : d->dying)
        delete widget.data();
}

QString Placement::taskId() const { return d->taskId; }

QString Placement::itemId() const { return d->itemId; }

QWidget *Placement::view() const { return d->view; }

QWidget *Placement::headerWidget() const { return d->header; }

CellStatus Placement::cellStatus() const { return d->cellStatus; }

bool Placement::hasStatus() const { return d->statusType() != nullptr; }

void Placement::refresh()
{
    if (hasStatus())
        d->services.status->refresh(d->itemId);
}

void Placement::openSettings()
{
    const Item *found = d->item();
    if (!found || !d->services.openServerSettings)
        return;
    d->services.openServerSettings(profiles::effective(*d->services.model, d->taskId, d->itemId),
                                   moduleOfType(found->type));
}

} // namespace mirante
