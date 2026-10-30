#pragma once

#include <mirante/widgets/StatusPill.h>

#include <QList>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QWidget>

#include <memory>
#include <optional>

namespace mirante::widgets {

struct ListColumn
{
    QString title;
    // 0 never hides. In a narrow cell, the highest number hides first. A column
    // that holds links takes 0.
    int hidePriority = 0;
    bool stretch = false;
};

struct ListLink
{
    QString id;                         // comes back in rowLinkActivated()
    QString label;
};

struct ListCell
{
    QString text;
    QVariant sortKey;                   // the text if null
    std::optional<StatusRole> role;     // shows the text as a pill
    QList<ListLink> links = {};         // shown instead of the text and the pill
};

struct ListRow
{
    QString key;                        // stable, so the selection survives new rows
    QList<ListCell> cells;
};

// A sortable table, with optional filters above it, pages below it, and links on
// its rows. Sorting and filtering cover all rows, and the pages cut up the result.
// See decision 0071.
class ItemListView : public QWidget
{
    Q_OBJECT

public:
    explicit ItemListView(QWidget *parent = nullptr);
    ~ItemListView() override;

    // Sorting starts on column sortColumn, descending.
    void setColumns(const QList<ListColumn> &columns, int sortColumn);
    void setColumnVisible(int column, bool visible);
    void setRows(const QList<ListRow> &rows);
    void setCell(const QString &key, int column, const ListCell &cell);

    // How many rows show at a time, with Previous and Next under the table. Zero
    // or less means no pages, which is the default. The controls hide when all
    // rows fit. A new sort or filter goes back to the first page. New rows stay
    // on the same page, or on the last one if there are now fewer pages.
    void setPageSize(int rows);
    int pageSize() const;

    // The keys of the current page's rows, in table order.
    QStringList visibleKeys() const;

    // A drop-down above the table for a column, filled from all rows' text in
    // that column, with "All" first. The choice survives setRows() while its
    // value exists, and otherwise falls back to "All". Its object name is
    // filter.<column>.
    void addFilter(int column, const QString &label);

    // The "Web" link in a thin row above the table. An empty url hides it.
    void setWebLink(const QUrl &url);
    QUrl webLink() const;

    // Shown instead of the table, such as "Loading…". Empty removes it.
    void setMessage(const QString &text);

signals:
    void rowMenuRequested(const QString &key, const QPoint &globalPos);
    // A link in a cell was clicked, or Return was pressed on its row, which
    // activates the row's first link.
    void rowLinkActivated(const QString &key, const QString &linkId);
    // visibleKeys() has a new value, for a view that asks more about those rows.
    void visibleKeysChanged();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante::widgets
