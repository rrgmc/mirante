// Tests ItemListView: sorting by key, the selection across setRows, hiding
// columns by width, the row menu, the "Web" link, the message, cells drawn as
// pills, links on rows, pages and filters.
#include <mirante/widgets/ItemListView.h>

#include <QComboBox>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTableView>
#include <QTest>
#include <QTextDocument>

#include <algorithm>

using namespace mirante::widgets;

namespace {

enum { RefColumn, TitleColumn, AuthorColumn, UpdatedColumn };

QList<ListColumn> fourColumns()
{
    return {{QStringLiteral("Ref"), 3, false},
            {QStringLiteral("Title"), 0, true},
            {QStringLiteral("Author"), 2, false},
            {QStringLiteral("Updated"), 1, false}};
}

// The updated column's sort key is the number, and its text sorts the other way.
ListRow makeRow(const QString &key, int updated, const QString &title = QStringLiteral("A title"))
{
    ListRow row;
    row.key = key;
    row.cells = {{QStringLiteral("!") + key, {}, {}},
                 {title, {}, {}},
                 {QStringLiteral("someone"), {}, {}},
                 {QStringLiteral("%1 days ago").arg(1000 - updated), updated, {}}};
    return row;
}

QTableView *tableOf(const ItemListView &view) { return view.findChild<QTableView *>(); }

// The keys of the rows as shown, from the ref column.
QStringList shownRefs(const ItemListView &view)
{
    const QAbstractItemModel *model = tableOf(view)->model();
    QStringList refs;
    for (int row = 0; row < model->rowCount(); ++row)
        refs.append(model->index(row, RefColumn).data().toString());
    return refs;
}

void clickRow(const ItemListView &view, int row, int column = TitleColumn)
{
    QTableView *table = tableOf(view);
    const QModelIndex index = table->model()->index(row, column);
    QTest::mouseClick(table->viewport(), Qt::LeftButton, {}, table->visualRect(index).center());
}

// The key of the selected row, from its ref, or empty without a selection.
QString selectedKey(const ItemListView &view)
{
    const QModelIndexList selected = tableOf(view)->selectionModel()->selectedIndexes();
    return selected.isEmpty() ? QString()
                              : selected.first().siblingAtColumn(RefColumn).data().toString().mid(1);
}

enum { LinkTitleColumn, LinksColumn };

QList<ListColumn> linkColumns()
{
    return {{QStringLiteral("Title"), 1, true}, {QString(), 0, false}};
}

ListRow linkRow(const QString &key)
{
    ListRow row;
    row.key = key;
    row.cells = {{key, {}, {}},
                 {{}, {}, {}, {{QStringLiteral("web"), QStringLiteral("Web")},
                               {QStringLiteral("open"), QStringLiteral("Open")}}}};
    return row;
}

// Sends a move to the viewport without moving the real cursor.
void moveMouse(QWidget *viewport, const QPoint &pos)
{
    QMouseEvent event(QEvent::MouseMove, pos, viewport->mapToGlobal(pos), Qt::NoButton, Qt::NoButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &event);
}

// The middle of each link in a cell, found from where the cursor changes.
QList<QPoint> linkCentres(QTableView *table, int row, int column)
{
    const QRect cell = table->visualRect(table->model()->index(row, column));
    QList<QPoint> centres;
    int start = -1;
    for (int x = cell.left(); x <= cell.right() + 1; ++x) {
        const QPoint pos(x, cell.center().y());
        moveMouse(table->viewport(), pos);
        const bool onLink = table->viewport()->cursor().shape() == Qt::PointingHandCursor;
        if (onLink && start < 0)
            start = x;
        if (!onLink && start >= 0) {
            centres.append(QPoint((start + x) / 2, cell.center().y()));
            start = -1;
        }
    }
    moveMouse(table->viewport(), QPoint(cell.left(), cell.top()));
    return centres;
}

QList<ListRow> numberedRows(int count)
{
    QList<ListRow> rows;
    for (int i = 0; i < count; ++i)
        rows.append(makeRow(QString::number(i), i));
    return rows;
}

// A view of rows 0 to count - 1, sorted so that the highest number comes first.
void fill(ItemListView &view, int count, int pageSize)
{
    view.setColumns(fourColumns(), UpdatedColumn);
    view.setRows(numberedRows(count));
    view.setPageSize(pageSize);
}

QAbstractButton *previousButton(const ItemListView &view)
{
    return view.findChild<QAbstractButton *>(QStringLiteral("previousPage"));
}

QAbstractButton *nextButton(const ItemListView &view)
{
    return view.findChild<QAbstractButton *>(QStringLiteral("nextPage"));
}

QString pageLine(const ItemListView &view)
{
    return view.findChild<QLabel *>(QStringLiteral("pageLine"))->text();
}

// Rows whose author is the given text, for the filters.
ListRow authoredRow(const QString &key, int updated, const QString &author)
{
    ListRow row = makeRow(key, updated);
    row.cells[AuthorColumn].text = author;
    return row;
}

QComboBox *authorFilter(const ItemListView &view)
{
    return view.findChild<QComboBox *>(QStringLiteral("filter.%1").arg(AuthorColumn));
}

QStringList itemsOf(const QComboBox *box)
{
    QStringList items;
    for (int item = 0; item < box->count(); ++item)
        items.append(box->itemText(item));
    return items;
}

} // namespace

class TestItemListView : public QObject
{
    Q_OBJECT

private slots:
    void sortsNewestFirstByKeyOnTheGivenColumn()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({makeRow(QStringLiteral("old"), 9), makeRow(QStringLiteral("new"), 100),
                      makeRow(QStringLiteral("mid"), 20)});
        // As text, "9" would sort above "100".
        QCOMPARE(shownRefs(view),
                 (QStringList{QStringLiteral("!new"), QStringLiteral("!mid"), QStringLiteral("!old")}));
    }

    void clickingAHeaderSorts()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({makeRow(QStringLiteral("b"), 1), makeRow(QStringLiteral("a"), 2)});
        QCOMPARE(shownRefs(view), (QStringList{QStringLiteral("!a"), QStringLiteral("!b")}));

        tableOf(view)->sortByColumn(UpdatedColumn, Qt::AscendingOrder);
        QCOMPARE(shownRefs(view), (QStringList{QStringLiteral("!b"), QStringLiteral("!a")}));
    }

    void cellsWithoutAKeySortByTextIgnoringCase()
    {
        ItemListView view;
        view.setColumns(fourColumns(), TitleColumn);
        view.setRows({makeRow(QStringLiteral("1"), 0, QStringLiteral("apple")),
                      makeRow(QStringLiteral("2"), 0, QStringLiteral("Banana")),
                      makeRow(QStringLiteral("3"), 0, QStringLiteral("cherry"))});
        QCOMPARE(shownRefs(view),
                 (QStringList{QStringLiteral("!3"), QStringLiteral("!2"), QStringLiteral("!1")}));
    }

    void newRowsKeepTheSort()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({makeRow(QStringLiteral("a"), 1)});
        view.setRows({makeRow(QStringLiteral("a"), 1), makeRow(QStringLiteral("b"), 2),
                      makeRow(QStringLiteral("c"), 3)});
        QCOMPARE(shownRefs(view),
                 (QStringList{QStringLiteral("!c"), QStringLiteral("!b"), QStringLiteral("!a")}));
        view.setRows({makeRow(QStringLiteral("a"), 5), makeRow(QStringLiteral("c"), 3)});
        QCOMPARE(shownRefs(view), (QStringList{QStringLiteral("!a"), QStringLiteral("!c")}));
    }

    void setCellChangesAndResorts()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({makeRow(QStringLiteral("a"), 2), makeRow(QStringLiteral("b"), 1)});
        view.setCell(QStringLiteral("b"), UpdatedColumn, {QStringLiteral("now"), 5, {}});
        QCOMPARE(shownRefs(view), (QStringList{QStringLiteral("!b"), QStringLiteral("!a")}));
        QCOMPARE(tableOf(view)->model()->index(0, UpdatedColumn).data().toString(),
                 QStringLiteral("now"));

        view.setCell(QStringLiteral("unknown"), UpdatedColumn, {QStringLiteral("x"), 9, {}});
        view.setCell(QStringLiteral("a"), 99, {QStringLiteral("x"), 9, {}});
        QCOMPARE(shownRefs(view), (QStringList{QStringLiteral("!b"), QStringLiteral("!a")}));
    }

    void shortRowsShowEmptyCells()
    {
        ItemListView view;
        view.setColumns(fourColumns(), RefColumn);
        ListRow row;
        row.key = QStringLiteral("a");
        row.cells = {{QStringLiteral("!a"), {}, {}}};
        view.setRows({row});
        QCOMPARE(tableOf(view)->model()->index(0, AuthorColumn).data().toString(), QString());
    }

    void clickingARowSelectsIt()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({makeRow(QStringLiteral("a"), 2), makeRow(QStringLiteral("b"), 1)});
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QVERIFY(selectedKey(view).isEmpty());

        clickRow(view, 1);
        QCOMPARE(selectedKey(view), QStringLiteral("b"));
    }

    void selectionIsKeptAcrossSetRows()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({makeRow(QStringLiteral("a"), 3), makeRow(QStringLiteral("b"), 2),
                      makeRow(QStringLiteral("c"), 1)});
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        clickRow(view, 1);
        QCOMPARE(selectedKey(view), QStringLiteral("b"));

        // "b" moves to the top, and another row takes its old source position.
        view.setRows({makeRow(QStringLiteral("x"), 1), makeRow(QStringLiteral("b"), 9),
                      makeRow(QStringLiteral("y"), 2), makeRow(QStringLiteral("z"), 3)});
        QCOMPARE(selectedKey(view), QStringLiteral("b"));
        QCOMPARE(tableOf(view)->currentIndex().row(), 0);

        // Fewer rows, with the selected one moving within them.
        view.setRows({makeRow(QStringLiteral("b"), 1), makeRow(QStringLiteral("y"), 2)});
        QCOMPARE(selectedKey(view), QStringLiteral("b"));
    }

    void selectionEndsWhenItsRowGoes()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({makeRow(QStringLiteral("a"), 2), makeRow(QStringLiteral("b"), 1)});
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        clickRow(view, 0);
        QCOMPARE(selectedKey(view), QStringLiteral("a"));

        view.setRows({makeRow(QStringLiteral("b"), 1)});
        QVERIFY(selectedKey(view).isEmpty());
    }

    void aResortOfTheSameRowsKeepsTheSelection()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({makeRow(QStringLiteral("a"), 2), makeRow(QStringLiteral("b"), 1)});
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        clickRow(view, 1);

        view.setCell(QStringLiteral("b"), UpdatedColumn, {QStringLiteral("now"), 5, {}});
        QCOMPARE(selectedKey(view), QStringLiteral("b"));
    }

    void scrollPositionIsKeptAcrossSetRows()
    {
        QList<ListRow> rows;
        for (int i = 0; i < 100; ++i)
            rows.append(makeRow(QString::number(i), i));

        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows(rows);
        view.resize(800, 200);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTRY_VERIFY(tableOf(view)->verticalScrollBar()->maximum() > 20);

        clickRow(view, 0);
        tableOf(view)->verticalScrollBar()->setValue(20);
        view.setRows(rows);
        QCoreApplication::processEvents();
        QCOMPARE(tableOf(view)->verticalScrollBar()->value(), 20);
        QCOMPARE(selectedKey(view), QStringLiteral("99"));
    }

    void columnsHideByPriorityAsTheWidthFalls()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({makeRow(QStringLiteral("a"), 1)});
        QTableView *table = tableOf(view);
        const auto hidden = [table] {
            QList<int> columns;
            for (int column = 0; column < 4; ++column) {
                if (table->isColumnHidden(column))
                    columns.append(column);
            }
            return columns;
        };

        view.resize(1000, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTRY_COMPARE(hidden(), QList<int>{});

        // The column with the highest number hides first, and the title stays.
        view.resize(100, 400);
        QTRY_COMPARE(hidden(), (QList<int>{RefColumn, AuthorColumn, UpdatedColumn}));

        view.resize(1000, 400);
        QTRY_COMPARE(hidden(), QList<int>{});

        // Find the widths at which each column goes, from wide to narrow.
        QList<QList<int>> seen;
        for (int width = 1000; width >= 100; width -= 10) {
            view.resize(width, 400);
            QCoreApplication::processEvents();
            if (seen.isEmpty() || seen.last() != hidden())
                seen.append(hidden());
        }
        QCOMPARE(seen, (QList<QList<int>>{{},
                                          {RefColumn},
                                          {RefColumn, AuthorColumn},
                                          {RefColumn, AuthorColumn, UpdatedColumn}}));
    }

    void setColumnVisibleOverridesWidth()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({makeRow(QStringLiteral("a"), 1)});
        QTableView *table = tableOf(view);
        view.resize(1000, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QTRY_VERIFY(!table->isColumnHidden(AuthorColumn));

        view.setColumnVisible(AuthorColumn, false);
        QVERIFY(table->isColumnHidden(AuthorColumn));
        view.resize(1100, 400);
        QCoreApplication::processEvents();
        QVERIFY(table->isColumnHidden(AuthorColumn));

        view.setColumnVisible(AuthorColumn, true);
        QVERIFY(!table->isColumnHidden(AuthorColumn));

        // Asking for a column does not bring it back in a narrow cell.
        view.resize(100, 400);
        QTRY_VERIFY(table->isColumnHidden(AuthorColumn));
        view.setColumnVisible(AuthorColumn, true);
        QVERIFY(table->isColumnHidden(AuthorColumn));
    }

    // A tooltip must not read server text as markup.
    void tooltipIsPlainText()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({makeRow(QStringLiteral("a"), 2, QStringLiteral("<b>bold</b>"))});

        const QAbstractItemModel *model = tableOf(view)->model();
        const QModelIndex title = model->index(0, TitleColumn);
        QCOMPARE(title.data().toString(), QStringLiteral("<b>bold</b>"));
        QTextDocument shown;
        shown.setHtml(title.data(Qt::ToolTipRole).toString());
        QCOMPARE(shown.toPlainText(), QStringLiteral("<b>bold</b>"));
    }

    void rowMenuIsAskedForOnARowOnly()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({makeRow(QStringLiteral("a"), 1)});
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        QTableView *table = tableOf(view);
        QSignalSpy menu(&view, &ItemListView::rowMenuRequested);
        const QPoint onRow = table->visualRect(table->model()->index(0, TitleColumn)).center();
        emit table->customContextMenuRequested(onRow);
        QCOMPARE(menu.size(), 1);
        QCOMPARE(menu.first().at(0).toString(), QStringLiteral("a"));
        QCOMPARE(menu.first().at(1).toPoint(), table->viewport()->mapToGlobal(onRow));

        emit table->customContextMenuRequested(QPoint(5, table->viewport()->height() - 2));
        QCOMPARE(menu.size(), 1);
        // A menu does not select the row.
        QVERIFY(selectedKey(view).isEmpty());
    }

    void webLinkIsInAThinRowAboveTheTable()
    {
        ItemListView view;
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto *web = view.findChild<QPushButton *>(QStringLiteral("web"));
        QVERIFY(web);
        QVERIFY(!web->isVisible());

        const QUrl url(QStringLiteral("https://gitlab.example.com/a/b/-/merge_requests"));
        view.setWebLink(url);
        QCOMPARE(view.webLink(), url);
        QVERIFY(web->isVisible());
        QTRY_VERIFY(web->mapTo(&view, QPoint()).y() < tableOf(view)->mapTo(&view, QPoint()).y());

        view.setWebLink(QUrl());
        QVERIFY(view.webLink().isEmpty());
        QVERIFY(!web->isVisible());
    }

    void messageShowsInsteadOfTheTable()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        view.setMessage(QStringLiteral("Loading…"));
        QVERIFY(!tableOf(view)->isVisible());
        const auto labels = view.findChildren<QLabel *>();
        const auto message = std::find_if(labels.begin(), labels.end(), [](QLabel *label) {
            return label->text() == QStringLiteral("Loading…");
        });
        QVERIFY(message != labels.end());
        QVERIFY((*message)->isVisible());
        QCOMPARE((*message)->textFormat(), Qt::PlainText);

        view.setMessage(QString());
        QVERIFY(tableOf(view)->isVisible());
        QVERIFY(!(*message)->isVisible());
    }

    void aCellWithARoleIsDrawnAsAPill()
    {
        ItemListView view;
        view.setColumns({{QStringLiteral("Status"), 0, true}}, 0);
        ListRow row;
        row.key = QStringLiteral("a");
        row.cells = {{QStringLiteral("Failed"), {}, StatusRole::Bad}};
        view.setRows({row});
        view.resize(400, 200);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        QTableView *table = tableOf(view);
        const QColor fill = statusColours(StatusRole::Bad, table->palette()).fill;
        const QImage image = table->viewport()->grab().toImage();
        bool found = false;
        for (int y = 0; y < image.height() && !found; ++y) {
            for (int x = 0; x < image.width() && !found; ++x)
                found = image.pixelColor(x, y).rgb() == fill.rgb();
        }
        QVERIFY(found);
        QCOMPARE(table->model()->index(0, 0).data().toString(), QStringLiteral("Failed"));
    }

    void clickingALinkEmitsItsRowAndId()
    {
        ItemListView view;
        view.setColumns(linkColumns(), LinkTitleColumn);
        view.setRows({linkRow(QStringLiteral("b")), linkRow(QStringLiteral("a"))});
        view.resize(600, 300);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        QTableView *table = tableOf(view);
        const QList<QPoint> links = linkCentres(table, 1, LinksColumn);
        QCOMPARE(links.size(), 2);

        // Sorted descending, so "a" is the second row.
        QSignalSpy activated(&view, &ItemListView::rowLinkActivated);
        QTest::mouseClick(table->viewport(), Qt::LeftButton, {}, links.at(1));
        QCOMPARE(activated.size(), 1);
        QCOMPARE(activated.last().at(0).toString(), QStringLiteral("a"));
        QCOMPARE(activated.last().at(1).toString(), QStringLiteral("open"));

        QTest::mouseClick(table->viewport(), Qt::LeftButton, {}, links.at(0));
        QCOMPARE(activated.size(), 2);
        QCOMPARE(activated.last().at(1).toString(), QStringLiteral("web"));
    }

    void clicksBesideALinkDoNothing()
    {
        ItemListView view;
        view.setColumns(linkColumns(), LinkTitleColumn);
        view.setRows({linkRow(QStringLiteral("a"))});
        view.resize(600, 300);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        QTableView *table = tableOf(view);
        const QList<QPoint> links = linkCentres(table, 0, LinksColumn);
        QCOMPARE(links.size(), 2);
        QSignalSpy activated(&view, &ItemListView::rowLinkActivated);

        // The space between the links, the text of another cell, and a press
        // that ends elsewhere.
        const QPoint between((links.at(0).x() + links.at(1).x()) / 2, links.at(0).y());
        QTest::mouseClick(table->viewport(), Qt::LeftButton, {}, between);
        clickRow(view, 0, LinkTitleColumn);
        QTest::mousePress(table->viewport(), Qt::LeftButton, {}, links.at(0));
        QTest::mouseRelease(table->viewport(), Qt::LeftButton, {}, links.at(1));
        QCOMPARE(activated.size(), 0);

        // Another button.
        QTest::mouseClick(table->viewport(), Qt::RightButton, {}, links.at(0));
        QCOMPARE(activated.size(), 0);
    }

    void hoveringALinkChangesTheCursorAndTheDrawing()
    {
        ItemListView view;
        view.setColumns(linkColumns(), LinkTitleColumn);
        view.setRows({linkRow(QStringLiteral("a"))});
        view.resize(600, 300);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        QTableView *table = tableOf(view);
        QWidget *viewport = table->viewport();
        const QList<QPoint> links = linkCentres(table, 0, LinksColumn);
        QCOMPARE(links.size(), 2);
        const QRect titleCell = table->visualRect(table->model()->index(0, LinkTitleColumn));
        moveMouse(viewport, titleCell.center());
        QCOMPARE(viewport->cursor().shape(), Qt::ArrowCursor);

        const QRect linksCell = table->visualRect(table->model()->index(0, LinksColumn));
        const QImage plain = viewport->grab(linksCell).toImage();
        moveMouse(viewport, links.at(0));
        QCOMPARE(viewport->cursor().shape(), Qt::PointingHandCursor);
        QVERIFY(viewport->grab(linksCell).toImage() != plain);

        moveMouse(viewport, titleCell.center());
        QCOMPARE(viewport->cursor().shape(), Qt::ArrowCursor);
        QCOMPARE(viewport->grab(linksCell).toImage(), plain);
    }

    void returnActivatesTheFirstLinkOfTheCurrentRow()
    {
        ItemListView view;
        view.setColumns(linkColumns(), LinkTitleColumn);
        ListRow plain;
        plain.key = QStringLiteral("plain");
        plain.cells = {{QStringLiteral("plain"), {}, {}}};
        view.setRows({linkRow(QStringLiteral("a")), plain});
        view.resize(600, 300);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        QTableView *table = tableOf(view);
        QSignalSpy activated(&view, &ItemListView::rowLinkActivated);
        QTest::keyClick(table, Qt::Key_Return);
        QCOMPARE(activated.size(), 0);

        // Descending, so "plain" is first.
        table->setCurrentIndex(table->model()->index(0, LinkTitleColumn));
        QTest::keyClick(table, Qt::Key_Return);
        QCOMPARE(activated.size(), 0);

        table->setCurrentIndex(table->model()->index(1, LinkTitleColumn));
        QTest::keyClick(table, Qt::Key_Return);
        QCOMPARE(activated.size(), 1);
        QCOMPARE(activated.last().at(0).toString(), QStringLiteral("a"));
        QCOMPARE(activated.last().at(1).toString(), QStringLiteral("web"));
    }

    // A poll can change which row sits at a source position while the user is on it.
    void returnActivatesTheSameRowAfterNewRows()
    {
        ItemListView view;
        view.setColumns(linkColumns(), LinkTitleColumn);
        view.setRows({linkRow(QStringLiteral("a")), linkRow(QStringLiteral("b"))});
        view.resize(600, 300);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        // Descending, so "a" is second.
        QTableView *table = tableOf(view);
        table->setCurrentIndex(table->model()->index(1, LinkTitleColumn));
        view.setRows({linkRow(QStringLiteral("b")), linkRow(QStringLiteral("a"))});

        QSignalSpy activated(&view, &ItemListView::rowLinkActivated);
        QTest::keyClick(table, Qt::Key_Return);
        QCOMPARE(activated.size(), 1);
        QCOMPARE(activated.last().at(0).toString(), QStringLiteral("a"));
    }

    void aLinkColumnFitsItsLinksAndStaysInANarrowCell()
    {
        ItemListView view;
        view.setColumns(linkColumns(), LinkTitleColumn);
        view.setRows({linkRow(QStringLiteral("a"))});
        view.resize(600, 300);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        QTableView *table = tableOf(view);
        const QFontMetrics metrics = table->fontMetrics();
        QTRY_VERIFY(table->columnWidth(LinksColumn)
                    >= metrics.horizontalAdvance(QStringLiteral("Web"))
                            + metrics.horizontalAdvance(QStringLiteral("Open")));

        view.resize(50, 300);
        QTRY_VERIFY(table->isColumnHidden(LinkTitleColumn));
        QVERIFY(!table->isColumnHidden(LinksColumn));
    }

    void pagesCutTheSortedRows()
    {
        ItemListView view;
        fill(view, 5, 2);
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        QCOMPARE(view.pageSize(), 2);
        QCOMPARE(view.visibleKeys(), (QStringList{QStringLiteral("4"), QStringLiteral("3")}));
        QCOMPARE(pageLine(view), QStringLiteral("1-2 of 5"));
        QVERIFY(previousButton(view)->isVisible());
        QVERIFY(!previousButton(view)->isEnabled());
        QVERIFY(nextButton(view)->isEnabled());

        nextButton(view)->click();
        QCOMPARE(view.visibleKeys(), (QStringList{QStringLiteral("2"), QStringLiteral("1")}));
        QCOMPARE(pageLine(view), QStringLiteral("3-4 of 5"));
        QVERIFY(previousButton(view)->isEnabled());
        QVERIFY(nextButton(view)->isEnabled());

        // What the table shows is what visibleKeys() says.
        QTableView *table = tableOf(view);
        QList<int> shown;
        for (int row = 0; row < table->model()->rowCount(); ++row) {
            if (!table->isRowHidden(row))
                shown.append(row);
        }
        QCOMPARE(shown, (QList<int>{2, 3}));

        nextButton(view)->click();
        QCOMPARE(view.visibleKeys(), QStringList{QStringLiteral("0")});
        QCOMPARE(pageLine(view), QStringLiteral("5-5 of 5"));
        QVERIFY(!nextButton(view)->isEnabled());

        previousButton(view)->click();
        QCOMPARE(pageLine(view), QStringLiteral("3-4 of 5"));
    }

    void noPagesWithoutAPositiveSize()
    {
        ItemListView view;
        fill(view, 5, 0);
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QCOMPARE(view.visibleKeys().size(), 5);
        QVERIFY(!nextButton(view)->isVisible());
        QVERIFY(!previousButton(view)->isVisible());

        view.setPageSize(2);
        QCOMPARE(view.visibleKeys().size(), 2);
        QVERIFY(nextButton(view)->isVisible());

        nextButton(view)->click();
        view.setPageSize(-1);
        QCOMPARE(view.pageSize(), 0);
        QCOMPARE(view.visibleKeys().size(), 5);
        QVERIFY(!nextButton(view)->isVisible());
        QTableView *table = tableOf(view);
        for (int row = 0; row < 5; ++row)
            QVERIFY(!table->isRowHidden(row));
    }

    void pageControlsHideWhenEverythingFits()
    {
        ItemListView view;
        fill(view, 2, 2);
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QVERIFY(!nextButton(view)->isVisible());
        QCOMPARE(view.visibleKeys().size(), 2);

        view.setRows(numberedRows(3));
        QVERIFY(nextButton(view)->isVisible());
        view.setRows(numberedRows(1));
        QVERIFY(!nextButton(view)->isVisible());
        view.setRows({});
        QVERIFY(!nextButton(view)->isVisible());
        QVERIFY(view.visibleKeys().isEmpty());
    }

    void aNewSortCoversAllRowsAndGoesToTheFirstPage()
    {
        ItemListView view;
        fill(view, 5, 2);
        nextButton(view)->click();

        tableOf(view)->sortByColumn(UpdatedColumn, Qt::AscendingOrder);
        QCOMPARE(view.visibleKeys(), (QStringList{QStringLiteral("0"), QStringLiteral("1")}));
        QCOMPARE(pageLine(view), QStringLiteral("1-2 of 5"));
    }

    void newRowsKeepThePageOrFallBackToTheLastOne()
    {
        ItemListView view;
        fill(view, 6, 2);
        nextButton(view)->click();
        QCOMPARE(view.visibleKeys(), (QStringList{QStringLiteral("3"), QStringLiteral("2")}));

        // More rows at the top move the older ones down, and the page holds.
        view.setRows(numberedRows(8));
        QCOMPARE(view.visibleKeys(), (QStringList{QStringLiteral("5"), QStringLiteral("4")}));
        QCOMPARE(pageLine(view), QStringLiteral("3-4 of 8"));

        // The same rows in another source order show in the same table order.
        QList<ListRow> reversed = numberedRows(8);
        std::reverse(reversed.begin(), reversed.end());
        view.setRows(reversed);
        QCOMPARE(view.visibleKeys(), (QStringList{QStringLiteral("5"), QStringLiteral("4")}));

        nextButton(view)->click();
        nextButton(view)->click();
        QCOMPARE(pageLine(view), QStringLiteral("7-8 of 8"));
        view.setRows(numberedRows(3));
        QCOMPARE(view.visibleKeys(), QStringList{QStringLiteral("0")});
        QCOMPARE(pageLine(view), QStringLiteral("3-3 of 3"));
    }

    void setCellCanMoveARowToAnotherPage()
    {
        ItemListView view;
        fill(view, 4, 2);
        view.setCell(QStringLiteral("0"), UpdatedColumn, {QStringLiteral("now"), 100, {}});
        QCOMPARE(view.visibleKeys(), (QStringList{QStringLiteral("0"), QStringLiteral("3")}));
    }

    void aNewPageStartsAtItsTop()
    {
        ItemListView view;
        fill(view, 100, 50);
        view.resize(800, 200);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        QTableView *table = tableOf(view);
        QTRY_VERIFY(table->verticalScrollBar()->maximum() > 10);
        table->verticalScrollBar()->setValue(10);
        nextButton(view)->click();
        QTRY_COMPARE(table->rowAt(0), 50);
        QCOMPARE(table->verticalScrollBar()->value(), 0);

        // New rows do not move the view within a page.
        QTRY_VERIFY(table->verticalScrollBar()->maximum() > 10);
        table->verticalScrollBar()->setValue(10);
        view.setRows(numberedRows(100));
        QCoreApplication::processEvents();
        QCOMPARE(table->verticalScrollBar()->value(), 10);
    }

    void columnsMeasureTheRowsOfTheNewPage()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        QList<ListRow> rows = numberedRows(4);
        // On the second page, since this is the lowest number.
        rows[0].cells[AuthorColumn].text = QStringLiteral("someone with a really very long name indeed");
        view.setRows(rows);
        view.setPageSize(2);
        view.resize(1200, 300);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        QTableView *table = tableOf(view);
        QTRY_VERIFY(table->columnWidth(AuthorColumn) > 0);
        const int before = table->columnWidth(AuthorColumn);
        nextButton(view)->click();
        QTRY_VERIFY(table->columnWidth(AuthorColumn) > before);
    }

    void theSelectionStaysOnItsRowAcrossPages()
    {
        ItemListView view;
        fill(view, 5, 2);
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        clickRow(view, 0);
        QCOMPARE(selectedKey(view), QStringLiteral("4"));

        nextButton(view)->click();
        QCOMPARE(selectedKey(view), QStringLiteral("4"));
        previousButton(view)->click();
        QCOMPARE(selectedKey(view), QStringLiteral("4"));
    }

    void visibleKeysChangedFollowsTheKeys()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        QSignalSpy changed(&view, &ItemListView::visibleKeysChanged);
        view.setRows(numberedRows(5));
        QCOMPARE(changed.size(), 1);
        view.setPageSize(2);
        QCOMPARE(changed.size(), 2);
        nextButton(view)->click();
        QCOMPARE(changed.size(), 3);
        view.setRows(numberedRows(5));
        QCOMPARE(changed.size(), 3);
        view.setPageSize(2);
        QCOMPARE(changed.size(), 3);
    }

    void aFilterListsTheValuesOfItsColumn()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({authoredRow(QStringLiteral("a"), 1, QStringLiteral("carol")),
                      authoredRow(QStringLiteral("b"), 2, QStringLiteral("Bob")),
                      authoredRow(QStringLiteral("c"), 3, QStringLiteral("carol")),
                      authoredRow(QStringLiteral("d"), 4, QString())});
        view.addFilter(AuthorColumn, QStringLiteral("Author"));

        QComboBox *box = authorFilter(view);
        QVERIFY(box);
        QCOMPARE(itemsOf(box), (QStringList{QStringLiteral("All"), QStringLiteral("Bob"),
                                            QStringLiteral("carol")}));
        QCOMPARE(box->currentIndex(), 0);
        QCOMPARE(view.visibleKeys().size(), 4);

        box->setCurrentIndex(2);
        QCOMPARE(view.visibleKeys(), (QStringList{QStringLiteral("c"), QStringLiteral("a")}));

        box->setCurrentIndex(0);
        QCOMPARE(view.visibleKeys().size(), 4);
    }

    void aFilterTreatsServerTextAsText()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({authoredRow(QStringLiteral("a"), 1, QStringLiteral("<b>All</b>")),
                      authoredRow(QStringLiteral("b"), 2, QStringLiteral("All"))});
        view.addFilter(AuthorColumn, QStringLiteral("<i>Author</i>"));

        // A value called "All" is not the "All" entry.
        QComboBox *box = authorFilter(view);
        QCOMPARE(box->count(), 3);
        box->setCurrentIndex(box->findData(QStringLiteral("All")));
        QCOMPARE(view.visibleKeys(), QStringList{QStringLiteral("b")});
        box->setCurrentIndex(box->findText(QStringLiteral("<b>All</b>")));
        QCOMPARE(view.visibleKeys(), QStringList{QStringLiteral("a")});

        const auto labels = view.findChildren<QLabel *>();
        const auto caption = std::find_if(labels.begin(), labels.end(), [](QLabel *label) {
            return label->text() == QStringLiteral("<i>Author</i>");
        });
        QVERIFY(caption != labels.end());
        QCOMPARE((*caption)->textFormat(), Qt::PlainText);
    }

    void aFilterSurvivesSetRowsWhileItsValueExists()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.addFilter(AuthorColumn, QStringLiteral("Author"));
        QComboBox *box = authorFilter(view);
        QCOMPARE(itemsOf(box), QStringList{QStringLiteral("All")});

        view.setRows({authoredRow(QStringLiteral("a"), 1, QStringLiteral("carol")),
                      authoredRow(QStringLiteral("b"), 2, QStringLiteral("bob"))});
        box->setCurrentIndex(box->findText(QStringLiteral("carol")));
        QCOMPARE(view.visibleKeys(), QStringList{QStringLiteral("a")});

        // A new value does not change the choice, though its place in the list moves.
        view.setRows({authoredRow(QStringLiteral("a"), 1, QStringLiteral("carol")),
                      authoredRow(QStringLiteral("c"), 3, QStringLiteral("carol")),
                      authoredRow(QStringLiteral("b"), 2, QStringLiteral("bob")),
                      authoredRow(QStringLiteral("d"), 4, QStringLiteral("alice"))});
        QCOMPARE(box->currentText(), QStringLiteral("carol"));
        QCOMPARE(view.visibleKeys(), (QStringList{QStringLiteral("c"), QStringLiteral("a")}));

        // Its value goes, and "All" takes over.
        view.setRows({authoredRow(QStringLiteral("b"), 2, QStringLiteral("bob"))});
        QCOMPARE(box->currentIndex(), 0);
        QCOMPARE(box->currentText(), QStringLiteral("All"));
        QCOMPARE(view.visibleKeys(), QStringList{QStringLiteral("b")});

        // It does not come back with its value.
        view.setRows({authoredRow(QStringLiteral("a"), 1, QStringLiteral("carol")),
                      authoredRow(QStringLiteral("b"), 2, QStringLiteral("bob"))});
        QCOMPARE(box->currentIndex(), 0);
        QCOMPARE(view.visibleKeys().size(), 2);
    }

    void filtersAndPagesWorkTogether()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        QList<ListRow> rows;
        for (int i = 0; i < 9; ++i) {
            rows.append(authoredRow(QString::number(i), i,
                                    i % 3 == 0 ? QStringLiteral("carol") : QStringLiteral("bob")));
        }
        view.setRows(rows);
        view.setPageSize(2);
        view.addFilter(AuthorColumn, QStringLiteral("Author"));
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QCOMPARE(pageLine(view), QStringLiteral("1-2 of 9"));

        nextButton(view)->click();
        QComboBox *box = authorFilter(view);
        box->setCurrentIndex(box->findText(QStringLiteral("carol")));
        // The filter sees rows that were on other pages, and starts at the top.
        QCOMPARE(view.visibleKeys(), (QStringList{QStringLiteral("6"), QStringLiteral("3")}));
        QCOMPARE(pageLine(view), QStringLiteral("1-2 of 3"));

        nextButton(view)->click();
        QCOMPARE(view.visibleKeys(), QStringList{QStringLiteral("0")});

        // The whole result fits on a page, so the controls go.
        view.setPageSize(3);
        QVERIFY(!nextButton(view)->isVisible());
        QCOMPARE(view.visibleKeys().size(), 3);

        box->setCurrentIndex(0);
        QVERIFY(nextButton(view)->isVisible());
        QCOMPARE(pageLine(view), QStringLiteral("1-3 of 9"));
    }

    void aFilteredOutSelectionEnds()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({authoredRow(QStringLiteral("a"), 2, QStringLiteral("carol")),
                      authoredRow(QStringLiteral("b"), 1, QStringLiteral("bob"))});
        view.addFilter(AuthorColumn, QStringLiteral("Author"));
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        clickRow(view, 0);
        QCOMPARE(selectedKey(view), QStringLiteral("a"));

        QComboBox *box = authorFilter(view);
        box->setCurrentIndex(box->findText(QStringLiteral("bob")));
        QVERIFY(selectedKey(view).isEmpty());
    }

    void setCellKeepsTheFilterInStep()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({authoredRow(QStringLiteral("a"), 2, QStringLiteral("carol")),
                      authoredRow(QStringLiteral("b"), 1, QStringLiteral("bob"))});
        view.addFilter(AuthorColumn, QStringLiteral("Author"));
        QComboBox *box = authorFilter(view);
        box->setCurrentIndex(box->findText(QStringLiteral("bob")));

        view.setCell(QStringLiteral("a"), AuthorColumn, {QStringLiteral("bob"), {}, {}});
        QCOMPARE(view.visibleKeys(), (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
        QCOMPARE(itemsOf(box), (QStringList{QStringLiteral("All"), QStringLiteral("bob")}));
    }

    void filtersAndPageControlsGiveWayToTheMessage()
    {
        ItemListView view;
        fill(view, 5, 2);
        view.addFilter(AuthorColumn, QStringLiteral("Author"));
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QVERIFY(authorFilter(view)->isVisible());
        QVERIFY(nextButton(view)->isVisible());
        QTableView *table = tableOf(view);
        QTRY_VERIFY(authorFilter(view)->mapTo(&view, QPoint()).y() < table->mapTo(&view, QPoint()).y());
        QTRY_VERIFY(nextButton(view)->mapTo(&view, QPoint()).y() > table->mapTo(&view, QPoint()).y());

        view.setMessage(QStringLiteral("Loading…"));
        QVERIFY(!authorFilter(view)->isVisible());
        QVERIFY(!nextButton(view)->isVisible());
    }
};

QTEST_MAIN(TestItemListView)
#include "tst_item_list_view.moc"
