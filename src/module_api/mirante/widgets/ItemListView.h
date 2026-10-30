#pragma once

#include <mirante/widgets/StatusPill.h>

#include <QList>
#include <QPoint>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <QWidget>

#include <memory>
#include <optional>

namespace mirante::widgets {

struct ListColumn
{
    QString title;
    // 0 never hides. In a narrow cell, the highest number hides first.
    int hidePriority = 0;
    bool stretch = false;
};

struct ListCell
{
    QString text;
    QVariant sortKey;                   // the text if null
    std::optional<StatusRole> role;     // shows the text as a pill
};

struct ListRow
{
    QString key;                        // stable, so the selection survives new rows
    QList<ListCell> cells;
};

// A sortable table with a preview of the selected row under a splitter. The
// preview stays hidden until the view is given one. See decision 0066.
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

    QString selectedKey() const;

    // The preview under the splitter. The view takes the widget. Null hides it.
    void setPreview(QWidget *preview);
    QWidget *preview() const;

    // The "Web" link in a thin row above the table. An empty url hides it.
    void setWebLink(const QUrl &url);
    QUrl webLink() const;

    // Shown instead of the table, such as "Loading…". Empty removes it.
    void setMessage(const QString &text);

signals:
    // A row was selected. Not sent when the selection only follows new rows,
    // or when it ends, as when its row goes or the preview's close button is
    // used. selectedKey() is then empty.
    void rowSelected(const QString &key);
    void rowMenuRequested(const QString &key, const QPoint &globalPos);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante::widgets
