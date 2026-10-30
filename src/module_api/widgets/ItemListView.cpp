#include <mirante/widgets/ItemListView.h>

#include <mirante/widgets/ActionRow.h>

#include <QAbstractTableModel>
#include <QApplication>
#include <QEvent>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPainter>
#include <QScrollBar>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace mirante::widgets {

namespace {

enum Role { SortKeyRole = Qt::UserRole, RowKeyRole, StatusColoursRole };

// The least room a stretching column, such as the title, needs before other
// columns start to hide.
constexpr int stretchColumnWidth = 160;
// The margins of a header section and its sort arrow.
constexpr int headerPadding = 28;
constexpr int pillPadding = 8;
constexpr int pillMargin = 4;
constexpr int pillVerticalMargin = 3;

class ListModel : public QAbstractTableModel
{
public:
    using QAbstractTableModel::QAbstractTableModel;

    void setColumns(const QList<ListColumn> &columns)
    {
        beginResetModel();
        m_columns = columns;
        endResetModel();
    }

    const QList<ListColumn> &columns() const { return m_columns; }

    // Changes the rows in place and sends no reset, which would make the view
    // and its header start over: scroll position, column sizes and hidden
    // columns. The caller puts the selection right afterwards.
    void setRows(const QList<ListRow> &rows)
    {
        const int oldCount = int(m_rows.size());
        const int newCount = int(rows.size());
        if (newCount < oldCount) {
            beginRemoveRows({}, newCount, oldCount - 1);
            m_rows.resize(newCount);
            endRemoveRows();
        }
        const int shared = std::min(oldCount, newCount);
        for (int row = 0; row < shared; ++row)
            m_rows[row] = rows[row];
        if (shared > 0 && !m_columns.isEmpty())
            emit dataChanged(index(0, 0), index(shared - 1, columnCount() - 1));
        if (newCount > oldCount) {
            beginInsertRows({}, oldCount, newCount - 1);
            m_rows.append(rows.mid(oldCount));
            endInsertRows();
        }
    }

    void setCell(const QString &key, int column, const ListCell &cell)
    {
        const int row = rowOf(key);
        if (row < 0 || column < 0 || column >= columnCount())
            return;
        QList<ListCell> &cells = m_rows[row].cells;
        if (cells.size() <= column)
            cells.resize(column + 1);
        cells[column] = cell;
        emit dataChanged(index(row, column), index(row, column));
    }

    int rowOf(const QString &key) const
    {
        for (int row = 0; row < m_rows.size(); ++row) {
            if (m_rows[row].key == key)
                return row;
        }
        return -1;
    }

    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : int(m_rows.size());
    }

    int columnCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : int(m_columns.size());
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid())
            return {};
        const ListRow &row = m_rows[index.row()];
        if (role == RowKeyRole)
            return row.key;
        const ListCell cell = row.cells.value(index.column());
        switch (role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            return cell.text;
        case SortKeyRole:
            return cell.sortKey.isNull() ? QVariant(cell.text) : cell.sortKey;
        case StatusColoursRole:
            return cell.role ? QVariant(int(*cell.role)) : QVariant();
        default:
            return {};
        }
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0
            && section < m_columns.size())
            return m_columns[section].title;
        return {};
    }

private:
    QList<ListColumn> m_columns;
    QList<ListRow> m_rows;
};

// Draws a cell with a status role as a pill. Other cells are drawn as usual.
class PillDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const QVariant role = index.data(StatusColoursRole);
        if (!role.isValid()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem cell(option);
        initStyleOption(&cell, index);
        const QString text = cell.text;
        // The background and the selection, without the text.
        cell.text.clear();
        QStyle *style = cell.widget ? cell.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &cell, painter, cell.widget);

        const StatusColours colours =
                statusColours(StatusRole(role.toInt()), option.palette);
        const QRect available = option.rect.adjusted(pillMargin, pillVerticalMargin, -pillMargin,
                                                     -pillVerticalMargin);
        const int wanted = option.fontMetrics.horizontalAdvance(text) + 2 * pillPadding;
        const QRect pill(available.topLeft(),
                         QSize(std::min(wanted, available.width()), available.height()));

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(colours.fill);
        painter->drawRoundedRect(pill, pill.height() / 2.0, pill.height() / 2.0);
        const QRect textArea = pill.adjusted(pillPadding, 0, -pillPadding, 0);
        painter->setPen(colours.text);
        painter->drawText(textArea, Qt::AlignCenter,
                          option.fontMetrics.elidedText(text, Qt::ElideRight, textArea.width()));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        if (index.data(StatusColoursRole).isValid()) {
            const int wanted = option.fontMetrics.horizontalAdvance(index.data().toString());
            size.setWidth(wanted + 2 * (pillPadding + pillMargin));
        }
        return size;
    }
};

// Removes a widget that its layout has already dropped. See the same function
// in SummaryView.cpp for why it is deleted later and loses its parent.
void discard(QWidget *widget)
{
    widget->hide();
    widget->setParent(nullptr);
    widget->deleteLater();
}

} // namespace

struct ItemListView::Private : public QObject
{
    ListModel *model = nullptr;
    QSortFilterProxyModel *proxy = nullptr;
    QTableView *table = nullptr;
    QSplitter *splitter = nullptr;
    QWidget *previewArea = nullptr;
    QVBoxLayout *previewLayout = nullptr;
    QWidget *preview = nullptr;
    ActionRow *webRow = nullptr;
    QLabel *message = nullptr;
    QSet<int> columnsHiddenByView;
    // The key of the last selected row, so that a signal goes out only when it
    // really changes.
    QString currentKey;
    // While rows change, the selection moves around and is put right after.
    bool changingRows = false;

    QString selectedKey() const
    {
        const QModelIndexList selected = table->selectionModel()->selectedIndexes();
        return selected.isEmpty() ? QString() : selected.first().data(RowKeyRole).toString();
    }

    void select(const QString &key)
    {
        const int row = key.isEmpty() ? -1 : model->rowOf(key);
        if (row < 0) {
            table->selectionModel()->clear();
            return;
        }
        table->selectionModel()->setCurrentIndex(proxy->mapFromSource(model->index(row, 0)),
                                                 QItemSelectionModel::ClearAndSelect
                                                         | QItemSelectionModel::Rows);
    }

    int neededWidth(int column) const
    {
        // QTableView makes this function protected, but the base class has it public.
        const QAbstractItemView *view = table;
        // Measured from the title, because sectionSizeHint() gives 0 for a hidden
        // section, and the result must not depend on what is hidden now.
        const QHeaderView *header = table->horizontalHeader();
        const int headerWidth = header->fontMetrics().horizontalAdvance(model->columns()[column].title)
                + headerPadding;
        const int width = std::max(headerWidth, view->sizeHintForColumn(column));
        return model->columns()[column].stretch ? std::max(width, stretchColumnWidth) : width;
    }

    // Hides columns by priority until the rest fit. It always starts from all
    // columns, so that the result depends only on the width.
    void updateColumns()
    {
        const QList<ListColumn> &columns = model->columns();
        QList<int> hideable;
        int needed = 0;
        for (int column = 0; column < columns.size(); ++column) {
            if (columnsHiddenByView.contains(column))
                continue;
            needed += neededWidth(column);
            if (columns[column].hidePriority > 0)
                hideable.append(column);
        }
        std::stable_sort(hideable.begin(), hideable.end(), [&columns](int a, int b) {
            return columns[a].hidePriority > columns[b].hidePriority;
        });

        QSet<int> tooNarrow;
        const int available = table->viewport()->width();
        for (int column : std::as_const(hideable)) {
            if (needed <= available)
                break;
            needed -= neededWidth(column);
            tooNarrow.insert(column);
        }
        for (int column = 0; column < columns.size(); ++column) {
            table->setColumnHidden(column, columnsHiddenByView.contains(column)
                                                   || tooNarrow.contains(column));
        }
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == table->viewport() && event->type() == QEvent::Resize)
            updateColumns();
        return QObject::eventFilter(watched, event);
    }
};

ItemListView::ItemListView(QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    d->webRow = new ActionRow(this);
    layout->addWidget(d->webRow);
    d->webRow->hide();

    d->model = new ListModel(this);
    d->proxy = new QSortFilterProxyModel(this);
    d->proxy->setSourceModel(d->model);
    d->proxy->setSortRole(SortKeyRole);
    d->proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

    d->splitter = new QSplitter(Qt::Vertical, this);
    d->splitter->setChildrenCollapsible(false);
    layout->addWidget(d->splitter, 1);

    d->table = new QTableView(d->splitter);
    d->table->setModel(d->proxy);
    d->table->setItemDelegate(new PillDelegate(d->table));
    d->table->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->table->setSelectionMode(QAbstractItemView::SingleSelection);
    d->table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    d->table->setShowGrid(false);
    d->table->setWordWrap(false);
    d->table->setAlternatingRowColors(true);
    d->table->setSortingEnabled(true);
    d->table->verticalHeader()->hide();
    d->table->setContextMenuPolicy(Qt::CustomContextMenu);
    d->table->viewport()->installEventFilter(d.get());

    connect(d->table->selectionModel(), &QItemSelectionModel::selectionChanged, d.get(), [this] {
        if (d->changingRows)
            return;
        const QString key = d->selectedKey();
        if (key == d->currentKey)
            return;
        d->currentKey = key;
        if (!key.isEmpty())
            emit rowSelected(key);
    });
    connect(d->table, &QWidget::customContextMenuRequested, d.get(), [this](const QPoint &pos) {
        const QModelIndex index = d->table->indexAt(pos);
        if (index.isValid())
            emit rowMenuRequested(index.data(RowKeyRole).toString(),
                                  d->table->viewport()->mapToGlobal(pos));
    });

    d->previewArea = new QWidget(d->splitter);
    d->previewLayout = new QVBoxLayout(d->previewArea);
    d->previewLayout->setContentsMargins(0, 0, 0, 0);
    auto *previewBar = new QHBoxLayout;
    previewBar->addStretch(1);
    auto *close = new QToolButton(d->previewArea);
    close->setObjectName(QStringLiteral("closePreview"));
    close->setText(tr("Close"));
    close->setToolTip(tr("Close the preview"));
    close->setAutoRaise(true);
    previewBar->addWidget(close);
    d->previewLayout->addLayout(previewBar);
    d->previewArea->hide();
    d->splitter->setStretchFactor(0, 3);
    d->splitter->setStretchFactor(1, 2);
    // Without a selection, a click on the same row brings the preview back.
    connect(close, &QToolButton::clicked, d.get(), [this] {
        setPreview(nullptr);
        d->table->selectionModel()->clear();
    });

    d->message = new QLabel(this);
    d->message->setTextFormat(Qt::PlainText);
    d->message->setWordWrap(true);
    d->message->setAlignment(Qt::AlignCenter);
    layout->addWidget(d->message, 1);
    d->message->hide();
}

ItemListView::~ItemListView() = default;

void ItemListView::setColumns(const QList<ListColumn> &columns, int sortColumn)
{
    d->columnsHiddenByView.clear();
    d->model->setColumns(columns);

    QHeaderView *header = d->table->horizontalHeader();
    bool anyStretch = false;
    for (int column = 0; column < columns.size(); ++column) {
        anyStretch = anyStretch || columns[column].stretch;
        header->setSectionResizeMode(column, columns[column].stretch ? QHeaderView::Stretch
                                                                     : QHeaderView::ResizeToContents);
    }
    header->setStretchLastSection(!anyStretch);
    d->table->sortByColumn(sortColumn, Qt::DescendingOrder);
    d->updateColumns();
}

void ItemListView::setColumnVisible(int column, bool visible)
{
    if (visible)
        d->columnsHiddenByView.remove(column);
    else
        d->columnsHiddenByView.insert(column);
    d->updateColumns();
}

void ItemListView::setRows(const QList<ListRow> &rows)
{
    const QString key = d->selectedKey();
    // Selecting a row scrolls to it, but new rows should not move the view.
    const int position = d->table->verticalScrollBar()->value();

    d->changingRows = true;
    d->model->setRows(rows);
    d->select(key);
    d->changingRows = false;
    d->currentKey = d->selectedKey();

    d->table->verticalScrollBar()->setValue(position);
    d->updateColumns();
}

void ItemListView::setCell(const QString &key, int column, const ListCell &cell)
{
    d->model->setCell(key, column, cell);
    d->updateColumns();
}

QString ItemListView::selectedKey() const { return d->selectedKey(); }

void ItemListView::setPreview(QWidget *preview)
{
    if (preview == d->preview)
        return;
    if (d->preview) {
        d->previewLayout->removeWidget(d->preview);
        discard(d->preview);
    }
    d->preview = preview;
    if (preview) {
        d->previewLayout->addWidget(preview, 1);
        preview->show();
    }
    d->previewArea->setVisible(preview != nullptr);
}

QWidget *ItemListView::preview() const { return d->preview; }

void ItemListView::setWebLink(const QUrl &url)
{
    d->webRow->setWebLink(url);
    d->webRow->setVisible(!url.isEmpty());
}

QUrl ItemListView::webLink() const { return d->webRow->webLink(); }

void ItemListView::setMessage(const QString &text)
{
    d->message->setText(text);
    d->message->setVisible(!text.isEmpty());
    d->splitter->setVisible(text.isEmpty());
}

} // namespace mirante::widgets
