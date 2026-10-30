// Tests ItemListView: sorting by key, the selection across setRows, hiding
// columns by width, the preview hiding and returning, the row menu, the "Web"
// link, the message, and cells drawn as pills.
#include <mirante/widgets/ItemListView.h>

#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSplitter>
#include <QTableView>
#include <QTest>
#include <QToolButton>

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

QToolButton *closeButton(const ItemListView &view)
{
    return view.findChild<QToolButton *>(QStringLiteral("closePreview"));
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
        QVERIFY(view.selectedKey().isEmpty());

        QSignalSpy selected(&view, &ItemListView::rowSelected);
        clickRow(view, 1);
        QCOMPARE(view.selectedKey(), QStringLiteral("b"));
        QCOMPARE(selected.size(), 1);
        QCOMPARE(selected.first().first().toString(), QStringLiteral("b"));
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
        QCOMPARE(view.selectedKey(), QStringLiteral("b"));

        QSignalSpy selected(&view, &ItemListView::rowSelected);
        // "b" moves to the top, and another row takes its old source position.
        view.setRows({makeRow(QStringLiteral("x"), 1), makeRow(QStringLiteral("b"), 9),
                      makeRow(QStringLiteral("y"), 2), makeRow(QStringLiteral("z"), 3)});
        QCOMPARE(view.selectedKey(), QStringLiteral("b"));
        QCOMPARE(tableOf(view)->currentIndex().row(), 0);
        QCOMPARE(selected.size(), 0);

        // Fewer rows, with the selected one moving within them.
        view.setRows({makeRow(QStringLiteral("b"), 1), makeRow(QStringLiteral("y"), 2)});
        QCOMPARE(view.selectedKey(), QStringLiteral("b"));
        QCOMPARE(selected.size(), 0);
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
        QCOMPARE(view.selectedKey(), QStringLiteral("a"));

        QSignalSpy selected(&view, &ItemListView::rowSelected);
        view.setRows({makeRow(QStringLiteral("b"), 1)});
        QVERIFY(view.selectedKey().isEmpty());
        QCOMPARE(selected.size(), 0);
    }

    void aResortOfTheSameRowsSendsNoSignal()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({makeRow(QStringLiteral("a"), 2), makeRow(QStringLiteral("b"), 1)});
        view.resize(800, 400);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        clickRow(view, 1);

        QSignalSpy selected(&view, &ItemListView::rowSelected);
        view.setCell(QStringLiteral("b"), UpdatedColumn, {QStringLiteral("now"), 5, {}});
        QCOMPARE(view.selectedKey(), QStringLiteral("b"));
        QCOMPARE(selected.size(), 0);
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
        QCOMPARE(view.selectedKey(), QStringLiteral("99"));
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

    void previewHidesAndReturns()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({makeRow(QStringLiteral("a"), 2), makeRow(QStringLiteral("b"), 1)});
        view.resize(800, 600);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        QVERIFY(!view.preview());
        QVERIFY(!closeButton(view)->isVisible());

        auto *first = new QLabel(QStringLiteral("First"));
        view.setPreview(first);
        QCOMPARE(view.preview(), first);
        QVERIFY(first->isVisible());
        QVERIFY(closeButton(view)->isVisible());

        // Replacing deletes the old one.
        QPointer<QLabel> old = first;
        auto *second = new QLabel(QStringLiteral("Second"));
        view.setPreview(second);
        QCOMPARE(view.preview(), second);
        QVERIFY(old && !old->parent());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!old);

        // Null hides it.
        view.setPreview(nullptr);
        QVERIFY(!view.preview());
        QVERIFY(!closeButton(view)->isVisible());

        view.setPreview(new QLabel(QStringLiteral("Third")));
        QVERIFY(view.preview()->isVisible());
        QVERIFY(closeButton(view)->isVisible());
    }

    void closingThePreviewLetsTheSameRowBringItBack()
    {
        ItemListView view;
        view.setColumns(fourColumns(), UpdatedColumn);
        view.setRows({makeRow(QStringLiteral("a"), 2), makeRow(QStringLiteral("b"), 1)});
        view.resize(800, 600);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        connect(&view, &ItemListView::rowSelected, &view,
                [&view](const QString &key) { view.setPreview(new QLabel(key)); });
        QSignalSpy selected(&view, &ItemListView::rowSelected);

        clickRow(view, 0);
        QVERIFY(view.preview());
        QCOMPARE(selected.size(), 1);

        closeButton(view)->click();
        QVERIFY(!view.preview());
        QVERIFY(view.selectedKey().isEmpty());
        QCOMPARE(selected.size(), 1);

        clickRow(view, 0);
        QVERIFY(view.preview());
        QCOMPARE(selected.size(), 2);
        QCOMPARE(selected.last().first().toString(), QStringLiteral("a"));
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
        QVERIFY(view.selectedKey().isEmpty());
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
};

QTEST_MAIN(TestItemListView)
#include "tst_item_list_view.moc"
