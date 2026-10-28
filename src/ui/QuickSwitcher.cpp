#include "ui/QuickSwitcher.h"

#include "core/FuzzyMatch.h"
#include "core/Model.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <optional>
#include <utility>

// The popup is a child of the parent window, so it moves with it and needs no
// window of its own. Focus stays in the line edit: the list never takes it, and
// the line edit forwards Up, Down, Enter and Escape. The popup hides when focus
// moves to a widget outside it.

namespace mirante {

namespace {

constexpr int kMaxVisibleRows = 10;
constexpr int kMaxWidth = 560;
constexpr int kMargin = 16;         // between the popup and the parent's edges
constexpr int kTopRatio = 8;        // the popup's top is at 1 / kTopRatio of the parent's height
constexpr int kRowPadding = 6;      // horizontal padding inside a row, and between its parts
constexpr int kVerticalPadding = 4;

enum Role {
    KindRole = Qt::UserRole + 1,
    DetailRole,
    PositionsRole,
};

// Order of kinds when scores are equal, and for an empty query.
int kindRank(SwitcherEntry::Kind kind)
{
    switch (kind) {
    case SwitcherEntry::Kind::Task:
        return 0;
    case SwitcherEntry::Kind::Item:
        return 1;
    case SwitcherEntry::Kind::Project:
        return 2;
    case SwitcherEntry::Kind::RootDashboard:
        return 3;
    case SwitcherEntry::Kind::Group:
        return 4;
    }
    return 5;
}

QString kindName(SwitcherEntry::Kind kind)
{
    switch (kind) {
    case SwitcherEntry::Kind::Group:
        return QuickSwitcher::tr("Group");
    case SwitcherEntry::Kind::Project:
        return QuickSwitcher::tr("Project");
    case SwitcherEntry::Kind::Task:
        return QuickSwitcher::tr("Task");
    case SwitcherEntry::Kind::Item:
        return QuickSwitcher::tr("Item");
    case SwitcherEntry::Kind::RootDashboard:
        return QuickSwitcher::tr("Dashboard");
    }
    return QString();
}

QString joinPath(const QString &a, const QString &b)
{
    return a + QStringLiteral(" / ") + b;
}

struct Ranked
{
    SwitcherEntry entry;
    FuzzyMatch match;
};

std::vector<Ranked> rankWithMatches(const std::vector<SwitcherEntry> &entries, const QString &query)
{
    std::vector<Ranked> ranked;
    for (const SwitcherEntry &entry : entries) {
        std::optional<FuzzyMatch> match = fuzzyMatch(query, entry.label);
        if (match)
            ranked.push_back({entry, std::move(*match)});
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked &a, const Ranked &b) {
        if (a.match.score != b.match.score)
            return a.match.score > b.match.score;
        const int ka = kindRank(a.entry.kind);
        const int kb = kindRank(b.entry.kind);
        if (ka != kb)
            return ka < kb;
        const int byLabel = QString::compare(a.entry.label, b.entry.label, Qt::CaseInsensitive);
        if (byLabel != 0)
            return byLabel < 0;
        return QString::compare(a.entry.detail, b.entry.detail, Qt::CaseInsensitive) < 0;
    });
    return ranked;
}

// Paints a row: the kind as a small dimmed tag, the label with its matched
// characters in bold, and the detail dimmed.
class RowDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex & /*index*/) const override
    {
        return QSize(0, option.fontMetrics.height() + 2 * kVerticalPadding);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.text.clear();
        const QWidget *widget = opt.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        const bool selected = opt.state.testFlag(QStyle::State_Selected);
        const QPalette::ColorGroup group =
            opt.state.testFlag(QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
        const QColor text = opt.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
        QColor dim = text;
        dim.setAlphaF(0.6f);

        painter->save();
        QRect rect = opt.rect.adjusted(kRowPadding, 0, -kRowPadding, 0);

        // The kind tag, in a column wide enough for the widest kind name.
        QFont small = opt.font;
        small.setPointSizeF(small.pointSizeF() * 0.85);
        const QFontMetrics smallMetrics(small);
        int tagWidth = 0;
        for (SwitcherEntry::Kind kind : {SwitcherEntry::Kind::Group, SwitcherEntry::Kind::Project,
                                         SwitcherEntry::Kind::Task, SwitcherEntry::Kind::Item,
                                         SwitcherEntry::Kind::RootDashboard})
            tagWidth = std::max(tagWidth, smallMetrics.horizontalAdvance(kindName(kind)));
        painter->setFont(small);
        painter->setPen(dim);
        painter->drawText(QRect(rect.left(), rect.top(), tagWidth, rect.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, index.data(KindRole).toString());
        rect.setLeft(rect.left() + tagWidth + 2 * kRowPadding);

        // The label, in runs of matched and unmatched characters.
        const QString label = index.data(Qt::DisplayRole).toString();
        std::vector<bool> matched(static_cast<std::size_t>(label.size()), false);
        for (const QVariant &position : index.data(PositionsRole).toList()) {
            const int i = position.toInt();
            if (i >= 0 && i < label.size()) {
                matched[static_cast<std::size_t>(i)] = true;
                if (label.at(i).isHighSurrogate() && i + 1 < label.size())
                    matched[static_cast<std::size_t>(i) + 1] = true;
            }
        }
        QFont bold = opt.font;
        bold.setBold(true);
        int x = rect.left();
        const int labelRight = rect.right();
        painter->setPen(text);
        qsizetype start = 0;
        while (start < label.size()) {
            const bool on = matched[static_cast<std::size_t>(start)];
            qsizetype end = start + 1;
            while (end < label.size() && matched[static_cast<std::size_t>(end)] == on)
                ++end;
            const QString run = label.mid(start, end - start);
            const QFont &font = on ? bold : opt.font;
            const QFontMetrics metrics(font);
            const int width = metrics.horizontalAdvance(run);
            painter->setFont(font);
            if (x + width > labelRight) {
                const QString elided = metrics.elidedText(run, Qt::ElideRight, labelRight - x);
                painter->drawText(QRect(x, rect.top(), labelRight - x, rect.height()),
                                  Qt::AlignLeft | Qt::AlignVCenter, elided);
                x = labelRight;
                break;
            }
            painter->drawText(QRect(x, rect.top(), width, rect.height()), Qt::AlignLeft | Qt::AlignVCenter, run);
            x += width;
            start = end;
        }

        // The detail, after the label, cut on the right if it does not fit.
        const QString detail = index.data(DetailRole).toString();
        x += 2 * kRowPadding;
        if (!detail.isEmpty() && x < rect.right()) {
            painter->setFont(opt.font);
            painter->setPen(dim);
            const QRect detailRect(x, rect.top(), rect.right() - x, rect.height());
            painter->drawText(detailRect, Qt::AlignLeft | Qt::AlignVCenter,
                              opt.fontMetrics.elidedText(detail, Qt::ElideRight, detailRect.width()));
        }
        painter->restore();
    }
};

// Calls a function for each event of the objects it watches. The function
// returns true to stop the event.
class EventFilter : public QObject
{
public:
    EventFilter(std::function<bool(QObject *, QEvent *)> filter, QObject *parent)
        : QObject(parent)
        , m_filter(std::move(filter))
    {
    }

    bool eventFilter(QObject *watched, QEvent *event) override { return m_filter(watched, event); }

private:
    std::function<bool(QObject *, QEvent *)> m_filter;
};

} // namespace

struct QuickSwitcher::Private
{
    QuickSwitcher *q = nullptr;
    const Model *model = nullptr;
    QLineEdit *edit = nullptr;
    QListWidget *list = nullptr;
    std::vector<SwitcherEntry> candidates;
    std::vector<SwitcherEntry> shown;

    void refilter();
    void moveSelection(int delta);
    void activateCurrent();
    void place();
    bool editKey(const QKeyEvent *event);
};

void QuickSwitcher::Private::refilter()
{
    list->clear();
    shown.clear();
    for (Ranked &ranked : rankWithMatches(candidates, edit->text())) {
        auto *row = new QListWidgetItem(ranked.entry.label, list);
        row->setData(KindRole, kindName(ranked.entry.kind));
        row->setData(DetailRole, ranked.entry.detail);
        QVariantList positions;
        for (int position : ranked.match.positions)
            positions.append(position);
        row->setData(PositionsRole, positions);
        shown.push_back(std::move(ranked.entry));
    }
    if (!shown.empty())
        list->setCurrentRow(0);
    place();
}

void QuickSwitcher::Private::moveSelection(int delta)
{
    const int count = list->count();
    if (count == 0)
        return;
    const int row = std::clamp(list->currentRow() + delta, 0, count - 1);
    list->setCurrentRow(row);
    list->scrollToItem(list->item(row));
}

void QuickSwitcher::Private::activateCurrent()
{
    const int row = list->currentRow();
    if (row < 0 || row >= static_cast<int>(shown.size()))
        return;
    const SwitcherEntry entry = shown[static_cast<std::size_t>(row)];
    q->hide();
    emit q->activated(entry);
}

// Centres the popup near the top of the parent, over a list of at most
// kMaxVisibleRows rows. The list is hidden when nothing matches.
void QuickSwitcher::Private::place()
{
    const QWidget *parent = q->parentWidget();
    if (!parent)
        return;
    const int rows = std::min(list->count(), kMaxVisibleRows);
    list->setVisible(rows > 0);
    const int rowHeight = rows > 0 ? list->sizeHintForRow(0) : 0;
    list->setFixedHeight(rows * rowHeight + 2 * list->frameWidth());

    const int width = std::max(0, std::min(kMaxWidth, parent->width() - 2 * kMargin));
    const QMargins margins = q->layout()->contentsMargins();
    const int height = edit->sizeHint().height() + (rows > 0 ? list->height() : 0) + margins.top()
                       + margins.bottom();
    q->setGeometry((parent->width() - width) / 2, parent->height() / kTopRatio, width, height);
}

bool QuickSwitcher::Private::editKey(const QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Down:
        moveSelection(1);
        return true;
    case Qt::Key_Up:
        moveSelection(-1);
        return true;
    case Qt::Key_PageDown:
        moveSelection(kMaxVisibleRows);
        return true;
    case Qt::Key_PageUp:
        moveSelection(-kMaxVisibleRows);
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        activateCurrent();
        return true;
    case Qt::Key_Escape:
        q->hide();
        return true;
    default:
        return false;
    }
}

QuickSwitcher::QuickSwitcher(const Model &model, QWidget *parent)
    : QFrame(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->model = &model;

    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);

    d->edit = new QLineEdit(this);
    d->edit->setPlaceholderText(tr("Go to a group, project, task, item or dashboard…"));
    d->edit->setFrame(false);

    d->list = new QListWidget(this);
    d->list->setFocusPolicy(Qt::NoFocus);
    d->list->setFrameShape(QFrame::NoFrame);
    d->list->setUniformItemSizes(true);
    d->list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d->list->setItemDelegate(new RowDelegate(d->list));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);
    layout->addWidget(d->edit);
    layout->addWidget(d->list);

    auto *filter = new EventFilter(
        [this](QObject *watched, QEvent *event) {
            if (watched == d->edit && event->type() == QEvent::KeyPress)
                return d->editKey(static_cast<QKeyEvent *>(event));
            if (watched == parentWidget() && event->type() == QEvent::Resize && isVisible())
                d->place();
            return false;
        },
        this);
    d->edit->installEventFilter(filter);
    if (parent)
        parent->installEventFilter(filter);

    connect(d->edit, &QLineEdit::textChanged, this, [this] { d->refilter(); });
    connect(d->list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        d->list->setCurrentItem(item);
        d->activateCurrent();
    });
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget * /*old*/, QWidget *now) {
        if (isVisible() && now && now != this && !isAncestorOf(now))
            hide();
    });

    hide();
}

QuickSwitcher::~QuickSwitcher() = default;

void QuickSwitcher::popup()
{
    d->candidates = candidates(*d->model);
    {
        const QSignalBlocker blocker(d->edit);
        d->edit->clear();
    }
    d->refilter();
    show();
    raise();
    d->edit->setFocus(Qt::PopupFocusReason);
}

std::vector<SwitcherEntry> QuickSwitcher::candidates(const Model &model)
{
    std::vector<SwitcherEntry> entries;
    for (const Group *group : model.groups()) {
        entries.push_back({SwitcherEntry::Kind::Group, group->id, QString(), group->name, QString()});
        for (const Project *project : model.projectsOfGroup(group->id)) {
            entries.push_back({SwitcherEntry::Kind::Project, project->id, QString(), project->name, group->name});
            for (const Task *task : model.tasksOfProject(project->id)) {
                entries.push_back({SwitcherEntry::Kind::Task, task->id, QString(), task->title,
                                   joinPath(group->name, project->name)});
                const QString itemDetail = joinPath(project->name, task->title);
                for (const Item &item : task->items)
                    entries.push_back({SwitcherEntry::Kind::Item, item.id, task->id, item.name, itemDetail});
            }
        }
    }
    for (const RootDashboard *dashboard : model.rootDashboards())
        entries.push_back({SwitcherEntry::Kind::RootDashboard, dashboard->id, QString(), dashboard->name,
                           tr("Dashboard")});
    return entries;
}

std::vector<SwitcherEntry> QuickSwitcher::rank(const std::vector<SwitcherEntry> &entries, const QString &query)
{
    std::vector<SwitcherEntry> result;
    for (Ranked &ranked : rankWithMatches(entries, query))
        result.push_back(std::move(ranked.entry));
    return result;
}

} // namespace mirante