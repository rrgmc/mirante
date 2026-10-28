#include "ui/RootDashboardsDialog.h"

#include "core/Model.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace mirante {

struct RootDashboardsDialog::Private
{
    RootDashboardsDialog *q = nullptr;
    Model *model = nullptr;
    std::function<bool(const QString &)> isOpen;
    std::function<bool(const QString &)> confirmDelete;
    QListWidget *list = nullptr;
    QPushButton *openButton = nullptr;
    QPushButton *deleteButton = nullptr;

    QString selectedId() const
    {
        const QListWidgetItem *item = list->currentItem();
        return item ? item->data(Qt::UserRole).toString() : QString();
    }

    // Fills the list in tab order, keeping the selection.
    void refresh()
    {
        const QString selected = selectedId();
        std::vector<const RootDashboard *> dashboards = model->rootDashboards();
        std::sort(dashboards.begin(), dashboards.end(), [](const RootDashboard *a, const RootDashboard *b) {
            return a->order != b->order ? a->order < b->order : a->id < b->id;
        });
        list->clear();
        for (const RootDashboard *dashboard : dashboards) {
            const bool open = isOpen && isOpen(dashboard->id);
            auto *item = new QListWidgetItem(open ? dashboard->name
                                                  : RootDashboardsDialog::tr("%1 (closed)").arg(dashboard->name));
            item->setData(Qt::UserRole, dashboard->id);
            list->addItem(item);
            if (dashboard->id == selected)
                list->setCurrentItem(item);
        }
        updateButtons();
    }

    void updateButtons()
    {
        const bool any = list->currentItem() != nullptr;
        openButton->setEnabled(any);
        deleteButton->setEnabled(any);
    }
};

RootDashboardsDialog::RootDashboardsDialog(Model &model, std::function<bool(const QString &id)> isOpen,
                                           QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->model = &model;
    d->isOpen = std::move(isOpen);
    d->confirmDelete = [this](const QString &name) {
        return QMessageBox::question(this, tr("Delete dashboard"),
                                     tr("Delete the root dashboard \"%1\"? The tasks and items it shows are kept.")
                                         .arg(name))
            == QMessageBox::Yes;
    };

    setWindowTitle(tr("Root dashboards"));
    resize(360, 420);

    d->list = new QListWidget(this);
    d->list->setObjectName(QStringLiteral("dashboardList"));
    d->openButton = new QPushButton(tr("Open"), this);
    d->deleteButton = new QPushButton(tr("Delete…"), this);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);

    auto *actions = new QHBoxLayout;
    actions->addWidget(d->openButton);
    actions->addWidget(d->deleteButton);
    actions->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(d->list, 1);
    layout->addLayout(actions);
    layout->addWidget(buttons);

    connect(d->list, &QListWidget::currentItemChanged, this, [this] { d->updateButtons(); });
    connect(d->list, &QListWidget::itemActivated, this, [this] { openSelected(); });
    connect(d->openButton, &QPushButton::clicked, this, &RootDashboardsDialog::openSelected);
    connect(d->deleteButton, &QPushButton::clicked, this, &RootDashboardsDialog::deleteSelected);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(&model, &Model::rootDashboardAdded, this, [this] { d->refresh(); });
    connect(&model, &Model::rootDashboardRemoved, this, [this] { d->refresh(); });
    connect(&model, &Model::modelReset, this, [this] { d->refresh(); });

    d->refresh();
}

RootDashboardsDialog::~RootDashboardsDialog()
{
    // The model and the children outlive d, so their signals must not reach it.
    disconnect(d->model, nullptr, this, nullptr);
    d->list->disconnect(this);
}

void RootDashboardsDialog::setConfirmDelete(std::function<bool(const QString &name)> confirm)
{
    d->confirmDelete = std::move(confirm);
}

QStringList RootDashboardsDialog::ids() const
{
    QStringList result;
    for (int row = 0; row < d->list->count(); ++row)
        result << d->list->item(row)->data(Qt::UserRole).toString();
    return result;
}

void RootDashboardsDialog::select(const QString &id)
{
    for (int row = 0; row < d->list->count(); ++row) {
        if (d->list->item(row)->data(Qt::UserRole).toString() == id)
            d->list->setCurrentRow(row);
    }
}

void RootDashboardsDialog::openSelected()
{
    const QString id = d->selectedId();
    if (!id.isEmpty())
        emit openRequested(id);
}

void RootDashboardsDialog::deleteSelected()
{
    const QString id = d->selectedId();
    const RootDashboard *dashboard = id.isEmpty() ? nullptr : d->model->rootDashboard(id);
    if (!dashboard || (d->confirmDelete && !d->confirmDelete(dashboard->name)))
        return;
    d->model->removeRootDashboard(id);
}

} // namespace mirante
