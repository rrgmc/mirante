#include "ui/WebProfilesDialog.h"

#include "core/Edits.h"
#include "core/Model.h"
#include "core/WebProfiles.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace mirante {

namespace {

// "1 group", "3 items". Spelled out so the text reads well without a translation.
QString countText(int count, const QString &one, const QString &many)
{
    return count == 1 ? one : many.arg(count);
}

} // namespace

struct WebProfilesDialog::Private
{
    WebProfilesDialog *q = nullptr;
    Model *model = nullptr;
    std::function<QString(const QString &, const QString &)> askName;
    std::function<bool(const QString &)> confirm;
    std::function<bool(const QString &)> confirmClear;
    QListWidget *list = nullptr;
    QPushButton *renameButton = nullptr;
    QPushButton *deleteButton = nullptr;
    QPushButton *clearButton = nullptr;
    QLabel *message = nullptr;

    QString selectedId() const
    {
        const QListWidgetItem *item = list->currentItem();
        return item ? item->data(Qt::UserRole).toString() : QString();
    }

    // Fills the list, keeping the selection, or selecting keep if given.
    void refresh(const QString &keep = {})
    {
        const QString selected = keep.isEmpty() ? selectedId() : keep;
        list->clear();
        for (const WebProfile &profile : webprofiles::all(*model)) {
            auto *item = new QListWidgetItem(profile.name, list);
            item->setData(Qt::UserRole, profile.id);
            if (profile.id == selected)
                list->setCurrentItem(item);
        }
        if (!list->currentItem() && list->count() > 0)
            list->setCurrentRow(0);
        updateButtons();
    }

    void updateButtons()
    {
        const QString id = selectedId();
        const bool editable = !id.isEmpty() && id != kDefaultWebProfile;
        renameButton->setEnabled(editable);
        deleteButton->setEnabled(editable);
        // Any profile can be cleared, Default too.
        clearButton->setEnabled(!id.isEmpty());
    }

    // Why name cannot be used for the profile with id, or an empty string.
    QString problemWith(const QString &name, const QString &id) const
    {
        if (name.isEmpty())
            return WebProfilesDialog::tr("Enter a name.");
        for (const WebProfile &profile : webprofiles::all(*model)) {
            if (profile.id != id && profile.name.compare(name, Qt::CaseInsensitive) == 0)
                return WebProfilesDialog::tr("A profile called \"%1\" already exists.").arg(profile.name);
        }
        return {};
    }

    // Shows why a name was refused, or hides the message.
    bool check(const QString &name, const QString &id)
    {
        const QString problem = problemWith(name, id);
        message->setText(problem);
        message->setVisible(!problem.isEmpty());
        return problem.isEmpty();
    }

    // How many groups and items name the profile.
    std::pair<int, int> users(const QString &id) const
    {
        int groups = 0;
        int items = 0;
        for (const Group *group : model->groups()) {
            if (group->webProfile == id)
                ++groups;
            for (const Project *project : model->projectsOfGroup(group->id)) {
                for (const Task *task : model->tasksOfProject(project->id)) {
                    for (const Item &item : task->items) {
                        if (item.webProfile == id)
                            ++items;
                    }
                }
            }
        }
        return {groups, items};
    }
};

WebProfilesDialog::WebProfilesDialog(Model &model, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->model = &model;
    d->askName = [this](const QString &title, const QString &text) {
        bool ok = false;
        const QString name = QInputDialog::getText(this, title, tr("Name:"), QLineEdit::Normal, text, &ok);
        return ok ? name : QString();
    };
    d->confirm = [this](const QString &question) {
        return QMessageBox::question(this, tr("Delete web profile"), question) == QMessageBox::Yes;
    };
    d->confirmClear = [this](const QString &name) {
        QMessageBox box(QMessageBox::Warning, tr("Clear web profile data"),
                        tr("Clear all data of the web profile \"%1\"?").arg(name), QMessageBox::NoButton, this);
        // The warning icon itself: the native alert on macOS would show the
        // app's icon instead.
        box.setIconPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning, nullptr, this).pixmap(64));
        box.setInformativeText(tr("Its logins, cookies, cache and site data on this device are deleted, so every "
                                  "site in it signs out. Pages in this profile reload. This cannot be undone."));
        QPushButton *clear = box.addButton(tr("Clear Data"), QMessageBox::DestructiveRole);
        QPushButton *cancel = box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(cancel);
        box.exec();
        return box.clickedButton() == clear;
    };

    setWindowTitle(tr("Web profiles"));
    resize(440, 440);

    auto *help = new QLabel(tr("Each group chooses a default profile, and each web item can choose its own. The "
                               "list is shared with the workspace; logins stay on this device."),
                            this);
    help->setWordWrap(true);

    // The warning, in a tinted box with the warning icon. It also says what
    // a profile keeps apart, so the help above does not repeat it.
    auto *warningBox = new QFrame(this);
    warningBox->setObjectName(QStringLiteral("warningBox"));
    warningBox->setStyleSheet(
        QStringLiteral("QFrame#warningBox { background: palette(alternate-base); border-radius: 6px; }"));
    auto *icon = new QLabel(warningBox);
    const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this) * 2;
    icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning, nullptr, this).pixmap(iconSize));
    icon->setAlignment(Qt::AlignTop);
    auto *warning = new QLabel(tr("Every item that uses a profile can read the logins and data of all sites in "
                                  "that profile. Use separate profiles for things you do not trust together."),
                               warningBox);
    warning->setObjectName(QStringLiteral("warning"));
    warning->setWordWrap(true);
    auto *warningLayout = new QHBoxLayout(warningBox);
    warningLayout->setContentsMargins(10, 8, 10, 8);
    warningLayout->setSpacing(10);
    warningLayout->addWidget(icon, 0, Qt::AlignTop);
    warningLayout->addWidget(warning, 1);

    d->list = new QListWidget(this);
    d->list->setObjectName(QStringLiteral("profileList"));

    auto *addButton = new QPushButton(tr("New profile…"), this);
    addButton->setObjectName(QStringLiteral("addProfile"));
    d->renameButton = new QPushButton(tr("Rename…"), this);
    d->renameButton->setObjectName(QStringLiteral("renameProfile"));
    d->deleteButton = new QPushButton(tr("Delete…"), this);
    d->deleteButton->setObjectName(QStringLiteral("deleteProfile"));
    d->clearButton = new QPushButton(tr("Clear Data…"), this);
    d->clearButton->setObjectName(QStringLiteral("clearProfile"));
    d->clearButton->setToolTip(tr("Sign out of every site in this profile, on this device"));

    d->message = new QLabel(this);
    d->message->setObjectName(QStringLiteral("message"));
    d->message->setWordWrap(true);
    d->message->hide();

    auto *rowButtons = new QHBoxLayout;
    rowButtons->addWidget(addButton);
    rowButtons->addWidget(d->renameButton);
    rowButtons->addWidget(d->deleteButton);
    rowButtons->addStretch(1);
    rowButtons->addWidget(d->clearButton);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(help);
    layout->addWidget(warningBox);
    layout->addWidget(d->list, 1);
    layout->addLayout(rowButtons);
    layout->addWidget(d->message);
    layout->addWidget(buttons);

    connect(addButton, &QPushButton::clicked, this, &WebProfilesDialog::addProfile);
    connect(d->renameButton, &QPushButton::clicked, this, &WebProfilesDialog::renameSelected);
    connect(d->deleteButton, &QPushButton::clicked, this, &WebProfilesDialog::deleteSelected);
    connect(d->clearButton, &QPushButton::clicked, this, &WebProfilesDialog::clearSelected);
    connect(d->list, &QListWidget::currentRowChanged, this, [this] { d->updateButtons(); });
    connect(d->list, &QListWidget::itemDoubleClicked, this, &WebProfilesDialog::renameSelected);
    // A reload or another device may change the list while the dialog is open.
    connect(&model, &Model::webProfilesChanged, this, [this] { d->refresh(); });
    connect(&model, &Model::modelReset, this, [this] { d->refresh(); });

    d->refresh();
}

WebProfilesDialog::~WebProfilesDialog()
{
    // The list outlives d, so its signals must not reach it.
    d->list->disconnect(this);
}

void WebProfilesDialog::setAskName(std::function<QString(const QString &, const QString &)> ask)
{
    d->askName = std::move(ask);
}

void WebProfilesDialog::setConfirm(std::function<bool(const QString &)> confirm) { d->confirm = std::move(confirm); }

void WebProfilesDialog::setConfirmClear(std::function<bool(const QString &)> confirm)
{
    d->confirmClear = std::move(confirm);
}

QStringList WebProfilesDialog::ids() const
{
    QStringList result;
    for (int i = 0; i < d->list->count(); ++i)
        result << d->list->item(i)->data(Qt::UserRole).toString();
    return result;
}

void WebProfilesDialog::select(const QString &id)
{
    for (int i = 0; i < d->list->count(); ++i) {
        if (d->list->item(i)->data(Qt::UserRole).toString() == id)
            d->list->setCurrentRow(i);
    }
}

void WebProfilesDialog::addProfile()
{
    const QString name = d->askName(tr("New web profile"), QString()).trimmed();
    if (name.isEmpty() || !d->check(name, QString()))
        return;
    WebProfile profile;
    profile.id = edits::newId();
    profile.name = name;
    std::vector<WebProfile> profiles = d->model->webProfiles();
    profiles.push_back(profile);
    d->model->setWebProfiles(profiles);
    d->refresh(profile.id);
}

void WebProfilesDialog::renameSelected()
{
    const QString id = d->selectedId();
    const WebProfile *current = d->model->webProfile(id);
    if (!current)
        return;
    const QString name = d->askName(tr("Rename web profile"), current->name).trimmed();
    if (name.isEmpty() || name == current->name || !d->check(name, id))
        return;
    std::vector<WebProfile> profiles = d->model->webProfiles();
    for (WebProfile &profile : profiles) {
        if (profile.id == id)
            profile.name = name;
    }
    d->model->setWebProfiles(profiles);
}

void WebProfilesDialog::deleteSelected()
{
    const QString id = d->selectedId();
    const WebProfile *current = d->model->webProfile(id);
    if (!current)
        return;
    const auto [groups, items] = d->users(id);
    QString question = tr("Delete the web profile \"%1\"? Its logins and site data on this device are deleted.")
                           .arg(current->name);
    if (groups > 0 || items > 0) {
        const QString users = groups > 0 && items > 0
            ? tr("%1 and %2").arg(countText(groups, tr("1 group"), tr("%1 groups")),
                                  countText(items, tr("1 item"), tr("%1 items")))
            : groups > 0 ? countText(groups, tr("1 group"), tr("%1 groups"))
                         : countText(items, tr("1 item"), tr("%1 items"));
        question += QLatin1Char(' ')
            + tr("It is used by %1. A group then uses Default, and an item its group's profile.").arg(users);
    }
    if (!d->confirm(question))
        return;
    d->message->hide();
    if (webprofiles::remove(*d->model, id))
        emit profileDeleted(id);
}

void WebProfilesDialog::clearSelected()
{
    const QString id = d->selectedId();
    if (id.isEmpty() || !d->confirmClear(webprofiles::nameOf(*d->model, id)))
        return;
    emit profileClearRequested(id);
}

} // namespace mirante
