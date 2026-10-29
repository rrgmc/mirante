// Tests Shortcuts on the fixtures: the focused cell, moving between cells,
// changing tabs and root tabs, maximising and closing.
#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "layout/CellFrame.h"
#include "layout/LayoutView.h"
#include "ui/MainWindow.h"
#include "ui/RootTabBar.h"
#include "ui/Shortcuts.h"
#include "ui/SlotTabBar.h"

#include <QApplication>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <memory>

using namespace mirante;

namespace {

const QString kFixLoginTask = QStringLiteral("760c8e20-8555-5a2c-bcbe-75e7fd08ca74");

// The "Work" dashboard of the task: a horizontal split of the session cell on
// the left and a tab group on the right. Its first tab is a vertical split of
// the issue over the merge request; the others are the pipeline and notes.
const QString kSession = QStringLiteral("3b610620-18e9-51b3-b413-51a7d696b733");
const QString kIssue = QStringLiteral("a2400d22-15bb-5bb0-be39-6a977ad25b0e");
const QString kMergeRequest = QStringLiteral("7152cdf9-61a0-535b-abb3-ea79c3f1ff0b");
const QString kPipeline = QStringLiteral("74147943-d9e2-59eb-9363-5ec6fdd175ee");
const QString kNotes = QStringLiteral("11039501-ba22-5677-86da-ea27842bf0b1");

// The registry has no modules, so every item has an unknown type.
// The Store is not opened, so nothing is written; the settings live in a
// temporary INI file.
struct Fixture
{
    QTemporaryDir dir;
    QSettings iniSettings{dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat};
    DeviceSettings settings{&iniSettings};
    ItemTypeRegistry types;
    Model model;
    Store dataStore{model};
    std::unique_ptr<MainWindow> window;

    Fixture()
    {
        const auto problems = store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model);
        if (!problems.empty())
            qWarning() << "fixture problems:" << problems.size();
        window = std::make_unique<MainWindow>(model, types, dataStore, settings);
    }

    bool showActive()
    {
        window->show();
        window->activateWindow();
        return QTest::qWaitForWindowActive(window.get());
    }

    Shortcuts *shortcuts() const { return window->shortcuts(); }
    RootTabBar *bar() const { return window->rootTabBar(); }
    SlotTabBar *slotBar() const { return window->slotTabBar(); }

    // The visible cell of the item in the current tab.
    CellFrame *cell(const QString &itemId) const
    {
        for (QWidget *widget : window->currentLayoutView()->visibleCells()) {
            auto *frame = qobject_cast<CellFrame *>(widget);
            if (frame && frame->ref() && frame->ref()->item == itemId)
                return frame;
        }
        return nullptr;
    }

    // Every visible cell has its size.
    bool laidOut() const
    {
        const auto cells = window->currentLayoutView()->visibleCells();
        return !cells.empty() && std::all_of(cells.begin(), cells.end(), [](const QWidget *cell) {
            return cell->isVisible() && cell->width() > 20 && cell->height() > 20;
        });
    }

    QString focusedItem() const
    {
        auto *frame = qobject_cast<CellFrame *>(shortcuts()->focusedCell());
        return frame && frame->ref() ? frame->ref()->item : QString();
    }

    void key(Qt::Key key, Qt::KeyboardModifiers modifiers)
    {
        QTest::keyClick(window.get(), key, modifiers);
    }
};

const Qt::KeyboardModifiers kMove = Qt::ControlModifier | Qt::AltModifier;

} // namespace

#define SHOW_ACTIVE(f)                                                                                                 \
    do {                                                                                                               \
        if (!(f).showActive())                                                                                         \
            QSKIP("The window cannot be activated on this platform.");                                                 \
    } while (false)

class TestShortcuts : public QObject
{
    Q_OBJECT

private slots:
    void focusMarksCell()
    {
        Fixture f;
        SHOW_ACTIVE(f);
        f.window->openTaskPreview(kFixLoginTask);
        CellFrame *session = f.cell(kSession);
        CellFrame *issue = f.cell(kIssue);
        QVERIFY(session && issue);

        f.shortcuts()->focusCell(session);
        QCOMPARE(f.shortcuts()->focusedCell(), session);
        QCOMPARE(QApplication::focusWidget(), session);
        QVERIFY(session->isCellFocused());

        issue->setFocus(Qt::MouseFocusReason);
        QTRY_COMPARE(f.shortcuts()->focusedCell(), issue);
        QVERIFY(issue->isCellFocused());
        QVERIFY(!session->isCellFocused());

        // Focus outside the cells clears the mark.
        f.bar()->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(!f.shortcuts()->focusedCell());
        QVERIFY(!issue->isCellFocused());
    }

    void moveBetweenCells()
    {
        Fixture f;
        SHOW_ACTIVE(f);
        f.window->openTaskPreview(kFixLoginTask);
        QTRY_VERIFY(f.laidOut());

        // With no focused cell, the first visible cell gets the focus.
        f.key(Qt::Key_Right, kMove);
        QCOMPARE(f.focusedItem(), kSession);

        f.key(Qt::Key_Right, kMove);
        QCOMPARE(f.focusedItem(), kIssue);
        f.key(Qt::Key_Right, kMove);     // nothing further right
        QCOMPARE(f.focusedItem(), kIssue);
        f.key(Qt::Key_Down, kMove);
        QCOMPARE(f.focusedItem(), kMergeRequest);
        f.key(Qt::Key_Up, kMove);
        QCOMPARE(f.focusedItem(), kIssue);
        f.key(Qt::Key_Down, kMove);
        f.key(Qt::Key_Left, kMove);
        QCOMPARE(f.focusedItem(), kSession);
        QCOMPARE(QApplication::focusWidget(), f.cell(kSession));
    }

    void changeTabInGroup()
    {
        Fixture f;
        SHOW_ACTIVE(f);
        f.window->openTaskPreview(kFixLoginTask);
        CellFrame *issue = f.cell(kIssue);
        QVERIFY(issue);
        QTabWidget *tabs = nullptr;
        for (QWidget *w = issue->parentWidget(); w && !tabs; w = w->parentWidget())
            tabs = qobject_cast<QTabWidget *>(w);
        QVERIFY(tabs);
        QCOMPARE(tabs->currentIndex(), 0);
        f.shortcuts()->focusCell(issue);

        f.key(Qt::Key_Tab, Qt::ControlModifier);
        QCOMPARE(tabs->currentIndex(), 1);
        QCOMPARE(f.focusedItem(), kPipeline);
        f.key(Qt::Key_Tab, Qt::ControlModifier);
        QCOMPARE(tabs->currentIndex(), 2);
        QCOMPARE(f.focusedItem(), kNotes);
        f.key(Qt::Key_Tab, Qt::ControlModifier);     // wraps
        QCOMPARE(tabs->currentIndex(), 0);
        QCOMPARE(f.focusedItem(), kIssue);

        f.key(Qt::Key_Backtab, Qt::ControlModifier | Qt::ShiftModifier);     // wraps back
        QCOMPARE(tabs->currentIndex(), 2);
        QCOMPARE(f.focusedItem(), kNotes);
        f.key(Qt::Key_Backtab, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(tabs->currentIndex(), 1);
    }

    // Ctrl+PgDown and Ctrl+PgUp go through the root tabs, then Focus, then
    // the preview, and wrap.
    void changeRootTab()
    {
        Fixture f;
        SHOW_ACTIVE(f);
        f.window->openTaskPreview(kFixLoginTask);
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::PreviewSlot));

        f.key(Qt::Key_PageDown, Qt::ControlModifier);     // wraps
        QCOMPARE(f.slotBar()->currentSlot(), -1);
        QCOMPARE(f.bar()->currentIndex(), 0);
        f.key(Qt::Key_PageDown, Qt::ControlModifier);
        QCOMPARE(f.bar()->currentIndex(), 1);
        f.key(Qt::Key_PageDown, Qt::ControlModifier);
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::FocusSlot));
        f.key(Qt::Key_PageUp, Qt::ControlModifier);
        QCOMPARE(f.slotBar()->currentSlot(), -1);
        QCOMPARE(f.bar()->currentIndex(), 1);
        f.key(Qt::Key_PageUp, Qt::ControlModifier);
        f.key(Qt::Key_PageUp, Qt::ControlModifier);     // wraps back
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::PreviewSlot));

        // A hidden tab is skipped.
        f.bar()->setTabVisible(0, false);
        f.key(Qt::Key_PageDown, Qt::ControlModifier);
        QCOMPARE(f.slotBar()->currentSlot(), -1);
        QCOMPARE(f.bar()->currentIndex(), 1);
    }

    void maximiseAndRestore()
    {
        Fixture f;
        SHOW_ACTIVE(f);
        f.window->openTaskPreview(kFixLoginTask);
        LayoutView *view = f.window->currentLayoutView();
        CellFrame *issue = f.cell(kIssue);
        QVERIFY(issue);
        QWidget *home = issue->parentWidget();
        const auto path = view->pathOf(issue);
        QVERIFY(path);
        f.shortcuts()->focusCell(issue);

        f.key(Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        QWidget *layer = issue->parentWidget();
        QVERIFY(layer);
        QCOMPARE(layer->objectName(), QStringLiteral("maximiseLayer"));
        QCOMPARE(layer->parentWidget(), f.window->contentArea());
        QVERIFY(layer->isVisible());
        QVERIFY(issue->isVisible());
        QCOMPARE(layer->geometry(), f.window->contentArea()->rect());
        QCOMPARE(f.shortcuts()->focusedCell(), issue);

        // The layer follows the content area's size.
        f.window->resize(f.window->width() + 40, f.window->height() + 30);
        QTRY_COMPARE(layer->geometry(), f.window->contentArea()->rect());

        f.key(Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(issue->parentWidget(), home);
        QVERIFY(!layer->isVisible());
        QCOMPARE(view->pathOf(issue), path);
        QCOMPARE(view->cellAt(*path), issue);
        QVERIFY(issue->isVisible());
        QCOMPARE(f.shortcuts()->focusedCell(), issue);
    }

    // The cell menu and the maximise button maximise their own cell, which
    // need not have the focus.
    void maximiseGivenCell()
    {
        Fixture f;
        SHOW_ACTIVE(f);
        f.window->openTaskPreview(kFixLoginTask);
        LayoutView *view = f.window->currentLayoutView();
        CellFrame *session = f.cell(kSession);
        CellFrame *issue = f.cell(kIssue);
        QVERIFY(session && issue);
        QWidget *sessionHome = session->parentWidget();
        QWidget *issueHome = issue->parentWidget();
        f.shortcuts()->focusCell(session);
        QVERIFY(!f.shortcuts()->isMaximised(issue));

        // Maximising focuses the cell.
        f.shortcuts()->toggleMaximised(issue);
        QCOMPARE(issue->parentWidget()->objectName(), QStringLiteral("maximiseLayer"));
        QVERIFY(f.shortcuts()->isMaximised(issue));
        QVERIFY(!f.shortcuts()->isMaximised(session));
        QCOMPARE(f.shortcuts()->focusedCell(), issue);

        // Maximising another cell restores the first.
        f.shortcuts()->toggleMaximised(session);
        QCOMPARE(issue->parentWidget(), issueHome);
        QVERIFY(view->pathOf(issue));
        QCOMPARE(session->parentWidget()->objectName(), QStringLiteral("maximiseLayer"));
        QVERIFY(f.shortcuts()->isMaximised(session));
        QVERIFY(!f.shortcuts()->isMaximised(issue));
        QCOMPARE(f.shortcuts()->focusedCell(), session);

        // The same cell again restores it.
        f.shortcuts()->toggleMaximised(session);
        QCOMPARE(session->parentWidget(), sessionHome);
        QVERIFY(!f.shortcuts()->isMaximised(session));
        QCOMPARE(f.shortcuts()->focusedCell(), session);

        // The shortcut restores a cell maximised this way, even when the focus
        // left it.
        f.shortcuts()->toggleMaximised(issue);
        f.bar()->setFocus(Qt::OtherFocusReason);
        QTRY_VERIFY(!f.shortcuts()->focusedCell());
        f.key(Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(issue->parentWidget(), issueHome);
        QVERIFY(!f.shortcuts()->isMaximised(issue));

        // Nothing happens for no cell.
        f.shortcuts()->toggleMaximised(nullptr);
        QVERIFY(!f.shortcuts()->isMaximised(nullptr));
    }

    void rootTabChangeRestores()
    {
        Fixture f;
        SHOW_ACTIVE(f);
        f.window->openTaskPreview(kFixLoginTask);
        LayoutView *view = f.window->currentLayoutView();
        CellFrame *issue = f.cell(kIssue);
        QVERIFY(issue);
        QWidget *home = issue->parentWidget();
        f.shortcuts()->focusCell(issue);

        f.key(Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(issue->parentWidget() != home);
        f.key(Qt::Key_PageDown, Qt::ControlModifier);
        QCOMPARE(issue->parentWidget(), home);
        QVERIFY(view->pathOf(issue));
    }

    void closeWhileMaximised()
    {
        Fixture f;
        SHOW_ACTIVE(f);
        f.window->openTaskPreview(kFixLoginTask);
        f.shortcuts()->focusCell(f.cell(kIssue));
        f.key(Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);

        // Ctrl+W closes the preview, and the window then goes with a cell
        // maximised in another tab.
        f.key(Qt::Key_W, Qt::ControlModifier);
        QCOMPARE(f.bar()->count(), 2);
        QVERIFY(!f.slotBar()->hasPreview());

        f.window->openTaskPreview(kFixLoginTask);
        f.shortcuts()->focusCell(f.cell(kMergeRequest));
        f.key(Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(f.cell(kMergeRequest)->parentWidget()->objectName(), QStringLiteral("maximiseLayer"));
        f.window.reset();
    }

    void closePreview()
    {
        Fixture f;
        SHOW_ACTIVE(f);
        f.window->openTaskPreview(kFixLoginTask);
        QVERIFY(f.slotBar()->hasPreview());
        f.key(Qt::Key_W, Qt::ControlModifier);
        QCOMPARE(f.bar()->count(), 2);
        QVERIFY(!f.slotBar()->hasPreview());
        QCOMPARE(f.bar()->currentIndex(), 0);
        QCOMPARE(f.slotBar()->currentSlot(), -1);

        // Focus cannot be closed.
        f.window->openFocus();
        f.key(Qt::Key_W, Qt::ControlModifier);
        QCOMPARE(f.bar()->count(), 2);
        QCOMPARE(f.slotBar()->currentSlot(), int(SlotTabBar::FocusSlot));
    }
};

QTEST_MAIN(TestShortcuts)
#include "tst_shortcuts.moc"
