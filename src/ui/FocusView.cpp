#include "ui/FocusView.h"

#include "core/Model.h"
#include "core/Sorting.h"
#include "ui/SidebarDelegate.h"
#include "ui/SidebarModel.h"

#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QListView>
#include <QPainter>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace mirante {

namespace {

// "project / group", painted dimmed after the title.
constexpr int contextRole = Qt::UserRole + 100;

// Opacity of the "project / group" text.
constexpr qreal contextOpacity = 0.55;

// Groups in manual order, as in the sidebar.
bool groupBefore(const Group *a, const Group *b)
{
    if (a->order != b->order)
        return a->order < b->order;
    if (const int c = QString::compare(a->name, b->name, Qt::CaseInsensitive); c != 0)
        return c < 0;
    return a->id < b->id;
}

std::vector<const Group *> sortedGroups(const Model &model)
{
    std::vector<const Group *> groups = model.groups();
    std::sort(groups.begin(), groups.end(), groupBefore);
    return groups;
}

// The sidebar look, with the priority marker and Low tasks dimmed, plus the
// task's project and group after the title.
class FocusDelegate : public SidebarDelegate
{
public:
    using SidebarDelegate::SidebarDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        SidebarDelegate::paint(painter, option, index);
        const QString context = index.data(contextRole).toString();
        if (context.isEmpty())
            return;

        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        const QWidget *widget = opt.widget;
        const QStyle *style = widget != nullptr ? widget->style() : QApplication::style();
        const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);
        const QFontMetrics metrics(opt.font);
        const int gap = metrics.horizontalAdvance(QLatin1Char(' ')) * 2;
        QRect contextRect = textRect;
        contextRect.setLeft(textRect.left() + style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, widget) + 1
                            + metrics.horizontalAdvance(opt.text) + gap);
        if (contextRect.width() <= 0)
            return;

        const bool selected = (opt.state & QStyle::State_Selected) != 0;
        const QPalette::ColorGroup group = (opt.state & QStyle::State_Active) != 0 ? QPalette::Active
                                                                                   : QPalette::Inactive;
        painter->save();
        painter->setOpacity(painter->opacity() * contextOpacity
                            * (index.data(SidebarModel::PriorityRole).toInt() == int(Priority::Low) ? 0.5 : 1.0));
        painter->setFont(opt.font);
        painter->setPen(opt.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text));
        painter->drawText(contextRect, Qt::AlignLeft | Qt::AlignVCenter,
                          metrics.elidedText(context, Qt::ElideRight, contextRect.width()));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize size = SidebarDelegate::sizeHint(option, index);
        const QString context = index.data(contextRole).toString();
        if (!context.isEmpty())
            size.rwidth() += QFontMetrics(option.font).horizontalAdvance(QStringLiteral("  ") + context);
        return size;
    }
};

// Emits through onOpen on Enter, instead of the view's own activation.
class FocusListView : public QListView
{
public:
    using QListView::QListView;

    std::function<void(const QModelIndex &)> onOpen;

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && currentIndex().isValid()) {
            if (onOpen)
                onOpen(currentIndex());
            event->accept();
            return;
        }
        QListView::keyPressEvent(event);
    }
};

} // namespace

struct FocusView::Private
{
    const Model &model;
    QString groupFilter;
    QComboBox *groups = nullptr;
    FocusListView *list = nullptr;
    QStandardItemModel *rows = nullptr;
    bool updatingGroups = false;

    explicit Private(const Model &m)
        : model(m)
    {
    }

    // Refills the combo box, keeping the chosen group if it still exists.
    void rebuildGroups()
    {
        updatingGroups = true;
        groups->clear();
        groups->addItem(FocusView::tr("All groups"), QString());
        for (const Group *group : sortedGroups(model))
            groups->addItem(group->name, group->id);
        int index = groups->findData(groupFilter);
        if (index < 0) {
            groupFilter.clear();
            index = 0;
        }
        groups->setCurrentIndex(index);
        updatingGroups = false;
    }

    // Refills the list, keeping the current task if it is still listed.
    void rebuildRows()
    {
        const QString current = list->currentIndex().data(SidebarModel::IdRole).toString();

        std::vector<const Task *> tasks;
        for (const Group *group : model.groups()) {
            if (!groupFilter.isEmpty() && group->id != groupFilter)
                continue;
            for (const Project *project : model.projectsOfGroup(group->id)) {
                const std::vector<const Task *> projectTasks = model.tasksOfProject(project->id);
                tasks.insert(tasks.end(), projectTasks.begin(), projectTasks.end());
            }
        }

        rows->clear();
        for (const Task *task : focusOrder(std::move(tasks), model)) {
            auto *row = new QStandardItem(task->title);
            row->setEditable(false);
            row->setData(int(SidebarModel::Kind::Task), SidebarModel::KindRole);
            row->setData(task->id, SidebarModel::IdRole);
            row->setData(task->id, SidebarModel::TaskIdRole);
            row->setData(int(task->priority), SidebarModel::PriorityRole);
            const Project *project = model.project(task->projectId);
            const Group *group = project != nullptr ? model.group(project->groupId) : nullptr;
            QString context = project != nullptr ? project->name : QString();
            if (group != nullptr)
                context += QStringLiteral(" / ") + group->name;
            row->setData(context, contextRole);
            row->setToolTip(context.isEmpty() ? task->title : task->title + QStringLiteral(" (") + context + ')');
            rows->appendRow(row);
        }

        for (int r = 0; r < rows->rowCount(); ++r) {
            if (rows->item(r)->data(SidebarModel::IdRole).toString() == current) {
                list->setCurrentIndex(rows->index(r, 0));
                break;
            }
        }
    }

    void rebuild()
    {
        rebuildGroups();
        rebuildRows();
    }
};

FocusView::FocusView(const Model &model, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>(model))
{
    d->rows = new QStandardItemModel(this);
    d->groups = new QComboBox(this);
    d->groups->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    d->list = new FocusListView(this);
    d->list->setModel(d->rows);
    d->list->setItemDelegate(new FocusDelegate(d->list));
    d->list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    d->list->setSelectionMode(QAbstractItemView::SingleSelection);
    d->list->setUniformItemSizes(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(d->groups, 0, Qt::AlignLeft);
    layout->addWidget(d->list, 1);

    const auto open = [this](const QModelIndex &index) {
        const QString id = index.data(SidebarModel::IdRole).toString();
        if (!id.isEmpty())
            emit taskOpened(id);
    };
    d->list->onOpen = open;
    connect(d->list, &QAbstractItemView::doubleClicked, this, open);

    connect(d->groups, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (d->updatingGroups || index < 0)
            return;
        setGroupFilter(d->groups->itemData(index).toString());
    });

    const auto rebuild = [this] { d->rebuild(); };
    const auto rebuildOn = [&](auto signal) { connect(&model, signal, this, rebuild); };
    rebuildOn(&Model::modelReset);
    rebuildOn(&Model::groupAdded);
    rebuildOn(&Model::groupChanged);
    rebuildOn(&Model::groupRemoved);
    rebuildOn(&Model::projectAdded);
    rebuildOn(&Model::projectChanged);
    rebuildOn(&Model::projectRemoved);
    rebuildOn(&Model::taskAdded);
    rebuildOn(&Model::taskChanged);
    rebuildOn(&Model::taskRemoved);

    d->rebuild();
}

FocusView::~FocusView() = default;

// An unknown group lists every group, as the combo box cannot show it.
void FocusView::setGroupFilter(const QString &groupId)
{
    if (groupId == d->groupFilter)
        return;
    d->groupFilter = groupId;
    d->rebuild();
}

QString FocusView::groupFilter() const { return d->groupFilter; }

QStringList FocusView::taskIds() const
{
    QStringList ids;
    for (int row = 0; row < d->rows->rowCount(); ++row)
        ids << d->rows->item(row)->data(SidebarModel::IdRole).toString();
    return ids;
}

} // namespace mirante
