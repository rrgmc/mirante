#include "ui/Sidebar.h"

#include "ui/SidebarDelegate.h"
#include "ui/SidebarModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QSet>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace mirante {

struct Sidebar::Private
{
    SidebarModel *model = nullptr;
    QTreeView *tree = nullptr;
    QToolButton *hideLow = nullptr;
    QToolButton *hideClosed = nullptr;

    // Expansion survives a reset of the model. Groups and projects start
    // expanded, so the user's collapsed ones are kept; tasks start collapsed.
    QSet<QString> collapsed;
    QSet<QString> expandedTasks;
    QString currentId;

    SidebarModel::Kind kind(const QModelIndex &index) const
    {
        return SidebarModel::Kind(index.data(SidebarModel::KindRole).toInt());
    }

    QString id(const QModelIndex &index) const { return index.data(SidebarModel::IdRole).toString(); }

    void restoreExpansion(const QModelIndex &parent)
    {
        const int rows = model->rowCount(parent);
        for (int row = 0; row < rows; ++row) {
            const QModelIndex index = model->index(row, 0, parent);
            const SidebarModel::Kind k = kind(index);
            if (k == SidebarModel::Kind::Item)
                continue;
            const QString entityId = id(index);
            const bool expand = k == SidebarModel::Kind::Task ? expandedTasks.contains(entityId)
                                                               : !collapsed.contains(entityId);
            if (expand)
                tree->expand(index);
            restoreExpansion(index);
        }
    }

    void applyFilter()
    {
        SortFilter filter = model->filter();
        filter.hideLow = hideLow->isChecked();
        filter.hideClosed = hideClosed->isChecked();
        model->setFilter(filter);
    }
};

Sidebar::Sidebar(const Model &model, const ItemTypeRegistry &types, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
    d->model = new SidebarModel(model, types, this);

    // Header: title and the collapse button.
    auto *title = new QLabel(QStringLiteral("Mirante"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto *collapse = new QToolButton(this);
    collapse->setText(QStringLiteral("«"));
    collapse->setToolTip(tr("Hide sidebar"));
    collapse->setAutoRaise(true);
    connect(collapse, &QToolButton::clicked, this, &Sidebar::collapseRequested);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(8, 4, 4, 4);
    header->addWidget(title, 1);
    header->addWidget(collapse);

    // The tree.
    d->tree = new QTreeView(this);
    d->tree->setModel(d->model);
    d->tree->setItemDelegate(new SidebarDelegate(d->tree));
    d->tree->setHeaderHidden(true);
    d->tree->setUniformRowHeights(true);
    d->tree->setFrameShape(QFrame::NoFrame);
    d->tree->setSelectionMode(QAbstractItemView::SingleSelection);
    d->tree->setDragEnabled(true);
    d->tree->setDragDropMode(QAbstractItemView::DragOnly);
    d->tree->setExpandsOnDoubleClick(false);    // a double click on a task pins it
    d->tree->setContextMenuPolicy(Qt::CustomContextMenu);

    // Footer: the filters.
    d->hideLow = new QToolButton(this);
    d->hideLow->setText(tr("Hide Low"));
    d->hideLow->setToolTip(tr("Hide Low projects and tasks"));
    d->hideLow->setCheckable(true);
    d->hideLow->setChecked(d->model->filter().hideLow);
    d->hideLow->setAutoRaise(true);

    d->hideClosed = new QToolButton(this);
    d->hideClosed->setText(tr("Hide done"));
    d->hideClosed->setToolTip(tr("Hide done and archived tasks"));
    d->hideClosed->setCheckable(true);
    d->hideClosed->setChecked(d->model->filter().hideClosed);
    d->hideClosed->setAutoRaise(true);

    connect(d->hideLow, &QToolButton::toggled, this, [this] { d->applyFilter(); });
    connect(d->hideClosed, &QToolButton::toggled, this, [this] { d->applyFilter(); });

    auto *footer = new QHBoxLayout;
    footer->setContentsMargins(4, 4, 4, 4);
    footer->addWidget(d->hideLow);
    footer->addWidget(d->hideClosed);
    footer->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(header);
    layout->addWidget(d->tree, 1);
    layout->addLayout(footer);

    // Expansion and the current row across resets.
    connect(d->tree, &QTreeView::collapsed, this, [this](const QModelIndex &index) {
        if (d->kind(index) == SidebarModel::Kind::Task)
            d->expandedTasks.remove(d->id(index));
        else
            d->collapsed.insert(d->id(index));
    });
    connect(d->tree, &QTreeView::expanded, this, [this](const QModelIndex &index) {
        if (d->kind(index) == SidebarModel::Kind::Task)
            d->expandedTasks.insert(d->id(index));
        else
            d->collapsed.remove(d->id(index));
    });
    connect(d->model, &QAbstractItemModel::modelAboutToBeReset, this,
            [this] { d->currentId = d->id(d->tree->currentIndex()); });
    connect(d->model, &QAbstractItemModel::modelReset, this, [this] {
        d->restoreExpansion({});
        const QModelIndex current = d->model->indexOf(d->currentId);
        if (current.isValid())
            d->tree->setCurrentIndex(current);
    });
    d->restoreExpansion({});

    // Clicks.
    connect(d->tree, &QTreeView::clicked, this, [this](const QModelIndex &index) {
        const QString taskId = index.data(SidebarModel::TaskIdRole).toString();
        switch (d->kind(index)) {
        case SidebarModel::Kind::Task:
            emit taskClicked(taskId);
            break;
        case SidebarModel::Kind::Item:
            emit itemClicked(taskId, d->id(index));
            break;
        default:
            break;
        }
    });
    connect(d->tree, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        if (d->kind(index) == SidebarModel::Kind::Task)
            emit taskDoubleClicked(d->id(index));
        else if (d->kind(index) != SidebarModel::Kind::Item)
            d->tree->setExpanded(index, !d->tree->isExpanded(index));
    });

    connect(d->tree, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const QModelIndex index = d->tree->indexAt(pos);
        if (!index.isValid() || d->kind(index) != SidebarModel::Kind::Task)
            return;
        const QString taskId = d->id(index);
        QMenu menu(this);
        menu.addAction(tr("Open in new root dashboard"), this,
                       [this, taskId] { emit openInNewRootDashboard(taskId); });
        menu.exec(d->tree->viewport()->mapToGlobal(pos));
    });
}

Sidebar::~Sidebar() = default;

void Sidebar::reveal(const QString &id)
{
    const QModelIndex index = d->model->indexOf(id);
    if (!index.isValid())
        return;
    for (QModelIndex parent = index.parent(); parent.isValid(); parent = parent.parent())
        d->tree->expand(parent);
    d->tree->setCurrentIndex(index);
    d->tree->scrollTo(index);
}

} // namespace mirante
