#include <mirante/widgets/ItemListView.h>

namespace mirante::widgets {

struct ItemListView::Private
{
    QWidget *preview = nullptr;
    QUrl webLink;
};

ItemListView::ItemListView(QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
}

ItemListView::~ItemListView() = default;

void ItemListView::setColumns(const QList<ListColumn> &columns, int sortColumn)
{
    Q_UNUSED(columns)
    Q_UNUSED(sortColumn)
}

void ItemListView::setColumnVisible(int column, bool visible)
{
    Q_UNUSED(column)
    Q_UNUSED(visible)
}

void ItemListView::setRows(const QList<ListRow> &rows) { Q_UNUSED(rows) }

void ItemListView::setCell(const QString &key, int column, const ListCell &cell)
{
    Q_UNUSED(key)
    Q_UNUSED(column)
    Q_UNUSED(cell)
}

QString ItemListView::selectedKey() const { return {}; }

void ItemListView::setPreview(QWidget *preview) { d->preview = preview; }

QWidget *ItemListView::preview() const { return d->preview; }

void ItemListView::setWebLink(const QUrl &url) { d->webLink = url; }

QUrl ItemListView::webLink() const { return d->webLink; }

void ItemListView::setMessage(const QString &text) { Q_UNUSED(text) }

} // namespace mirante::widgets
