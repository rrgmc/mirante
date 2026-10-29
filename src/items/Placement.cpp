#include "items/Placement.h"

#include "core/Model.h"
#include "core/Profiles.h"
#include "items/ItemContextImpl.h"
#include "items/ItemTypeRegistry.h"
#include "items/ViewPool.h"
#include "web/ProfileManager.h"

#include <mirante/ItemType.h>

#include <QHideEvent>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
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
    QLabel *typePlaceholder = nullptr;  // the placeholder of a type without a view, if shown
    bool gone = false;                  // the item or its task no longer exists
    QString waitingFor;                 // the profile being cleared, while the view waits for it
    bool inPool = false;
    QObject *watcher = nullptr;

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

    // Removes the view, its header widget and its pool entry, and shows
    // replacement instead.
    void dropView(QWidget *replacement)
    {
        if (inPool) {
            inPool = false;
            if (services.pool)
                services.pool->remove(q);
        }
        setHeader(nullptr);
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

        // Web views are heavy, so the pool may unload them while hidden.
        if (type->rendering() == Rendering::Web && services.pool) {
            inPool = true;
            services.pool->add(q, [this] {
                // The pool has already forgotten this placement.
                inPool = false;
                unload();
            });
            if (!q->isVisible())
                services.pool->setVisible(q, false);
        }
    }

    void onVisibilityChanged(bool shown)
    {
        // The first show makes the view, and so does a show after unloading
        // or after a change made while hidden.
        if (shown && !made && !gone)
            makeView();
        if (inPool && services.pool)
            services.pool->setVisible(q, shown);
    }

    // The user clears the data of a profile. A view in it goes at once, so
    // the profile can be released, and comes back when the clearing is done.
    void onProfileClearing(const QString &profileId)
    {
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
    // configuration or profile changed, or the module settings of
    // that profile, and dropped when the item is gone. A hidden placement
    // makes its new view when it is shown.
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
        } else if (found->type != madeType || found->config != madeConfig || profileOf(*found) != madeProfile
                   || profileDataOf(*found) != madeProfileData) {
            dropView(nullptr);
        } else {
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

    d->context.onHeaderWidget = [this](QWidget *widget) { d->setHeader(widget); };
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
            if (d->made && (!d->madeProfile.isEmpty() || !d->madeProfileData.isEmpty()))
                d->onModelChanged();
        };
        connect(model, &Model::groupChanged, this, profileMayChange);
        connect(model, &Model::projectChanged, this, profileMayChange);
        connect(model, &Model::profilesChanged, this, profileMayChange);
    }
    if (ProfileManager *profiles = services.profiles) {
        connect(profiles, &ProfileManager::profileClearing, this,
                [this](const QString &id) { d->onProfileClearing(id); });
        connect(profiles, &ProfileManager::profileCleared, this,
                [this](const QString &id) { d->onProfileCleared(id); });
    }
}

Placement::~Placement()
{
    // No more show or hide events reach d.
    delete d->watcher;
    if (d->inPool && d->services.pool)
        d->services.pool->remove(this);
    d->inPool = false;
    QObject::disconnect(d->headerDestroyed);
    // The views go while the context they may use still exists.
    delete d->view.data();
    for (const QPointer<QWidget> &widget : d->dying)
        delete widget.data();
}

QString Placement::taskId() const { return d->taskId; }

QString Placement::itemId() const { return d->itemId; }

QWidget *Placement::view() const { return d->view; }

QWidget *Placement::headerWidget() const { return d->header; }

} // namespace mirante
