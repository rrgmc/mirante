// Tests PageKeys: matching keys, reading them from an item, and the filter on
// a plain widget and on a real web page. The web page test also checks what
// decision 0088 relies on: with the focus in a page's text field, every app
// shortcut of docs/ui.md#keyboard still runs, except the editing keys that
// WebEngine keeps for the page, Ctrl+B among them.
#include "web/PageKeys.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QLineEdit>
#include <QShortcut>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>

#include <memory>

using namespace mirante;

namespace {

// Starting Chromium for the first page takes a while.
constexpr int kTimeout = 20000;

// The window shortcuts of docs/ui.md#keyboard and of the menus, on Windows
// and Linux.
QList<QKeySequence> appKeys()
{
    return {
        QKeySequence(Qt::CTRL | Qt::Key_K),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E),
        QKeySequence(Qt::CTRL | Qt::Key_D),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D),
        QKeySequence(Qt::CTRL | Qt::Key_P),
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Left),
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Up),
        QKeySequence(Qt::CTRL | Qt::Key_Tab),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Backtab),
        QKeySequence(Qt::CTRL | Qt::Key_PageDown),
        QKeySequence(Qt::CTRL | Qt::Key_PageUp),
        QKeySequence(Qt::Key_F11),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M),
        QKeySequence(Qt::Key_F2),
        QKeySequence(Qt::CTRL | Qt::Key_W),
        QKeySequence(Qt::CTRL | Qt::Key_N),
        QKeySequence(Qt::CTRL | Qt::Key_O),
        QKeySequence(Qt::Key_F5),
        QKeySequence(Qt::CTRL | Qt::Key_Q),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L),
    };
}

// Sends a key press and release, as typed, to the widget with the focus.
void type(QWidget *target, const QKeyCombination &key)
{
    QTest::keyClick(target, key.key(), key.keyboardModifiers());
}

} // namespace

class TestPageKeys : public QObject
{
    Q_OBJECT

    std::unique_ptr<QWebEngineProfile> profile;

    // A window with a shortcut for each key, counting how often each ran.
    struct Window
    {
        QWidget widget;
        QMap<QString, int> ran;

        explicit Window(const QList<QKeySequence> &keys)
        {
            for (const QKeySequence &key : keys) {
                auto *shortcut = new QShortcut(key, &widget);
                shortcut->setContext(Qt::WindowShortcut);
                const QString name = key.toString(QKeySequence::PortableText);
                QObject::connect(shortcut, &QShortcut::activated, &widget, [this, name] { ++ran[name]; });
            }
            new QVBoxLayout(&widget);
        }
    };

    static QString runJavaScript(QWebEnginePage *page, const QString &script)
    {
        bool done = false;
        QString result;
        page->runJavaScript(script, [&](const QVariant &value) {
            result = value.toString();
            done = true;
        });
        if (!QTest::qWaitFor([&] { return done; }, kTimeout))
            return QStringLiteral("<timeout>");
        return result;
    }

    // A page whose text field has the focus and records each key it gets. It
    // shows the window first, as the app does: the debug WebEngine stops on a
    // page loaded while hidden and shown later.
    std::unique_ptr<QWebEngineView> makePage(QWidget *parent)
    {
        auto view = std::make_unique<QWebEngineView>(profile.get(), parent);
        parent->layout()->addWidget(view.get());
        parent->show();
        QSignalSpy loads(view.get(), &QWebEngineView::loadFinished);
        view->setHtml(QStringLiteral("<input id=field autofocus><script>window.keys = [];"
                                     "document.addEventListener('keydown', e => {"
                                     "if (!['Control', 'Shift', 'Alt', 'Meta'].includes(e.key))"
                                     "window.keys.push((e.ctrlKey ? 'Ctrl+' : '') + e.key); }, true);</script>"));
        if (!loads.wait(kTimeout))
            return nullptr;
        return view;
    }

private slots:
    void initTestCase() { profile = std::make_unique<QWebEngineProfile>(); }

    void cleanupTestCase()
    {
        QTest::qWait(100);
        profile.reset();
    }

    void matches()
    {
        const QList<QKeySequence> keys = {QKeySequence(Qt::CTRL | Qt::Key_K), QKeySequence()};
        QVERIFY(PageKeys::matches(keys, Qt::CTRL | Qt::Key_K));
        QVERIFY(!PageKeys::matches(keys, Qt::CTRL | Qt::SHIFT | Qt::Key_K));
        QVERIFY(!PageKeys::matches(keys, QKeyCombination(Qt::Key_K)));
        QVERIFY(!PageKeys::matches({}, Qt::CTRL | Qt::Key_K));
    }

    void fromStrings()
    {
        const QList<QKeySequence> keys = PageKeys::fromStrings(
            {QStringLiteral("Ctrl+K"), QStringLiteral("Ctrl+Shift+P"), QStringLiteral(""), QStringLiteral("Bogus+")});
        QCOMPARE(keys, QList<QKeySequence>({QKeySequence(Qt::CTRL | Qt::Key_K),
                                            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P)}));
    }

    // On plain widgets: the key goes to the focused child, made after the
    // filter, instead of the window's shortcut. Other keys still run theirs.
    void plainWidget()
    {
        Window window({QKeySequence(Qt::CTRL | Qt::Key_K), QKeySequence(Qt::CTRL | Qt::Key_B),
                       QKeySequence(Qt::CTRL | Qt::Key_V)});
        auto *view = new QWidget(&window.widget);
        window.widget.layout()->addWidget(view);
        PageKeys::install(view, [] { return QList<QKeySequence>{QKeySequence(Qt::CTRL | Qt::Key_K)}; });
        auto *field = new QLineEdit(view);
        window.widget.show();
        window.widget.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window.widget));
        field->setFocus();
        QTRY_VERIFY(field->hasFocus());

        type(field, Qt::CTRL | Qt::Key_K);
        QCOMPARE(window.ran.value(QStringLiteral("Ctrl+K")), 0);
        type(field, Qt::CTRL | Qt::Key_B);
        QCOMPARE(window.ran.value(QStringLiteral("Ctrl+B")), 1);
        // A plain text field keeps its paste, as WebEngine does.
        type(field, Qt::CTRL | Qt::Key_V);
        QCOMPARE(window.ran.value(QStringLiteral("Ctrl+V")), 0);
    }

    // The check of decision 0088 on a real page.
    void webPage()
    {
        // Chromium's debug checks stop the test after these keys, on the
        // offscreen platform. The release build, as in CI, runs it. See
        // docs/web-views.md#debug-builds.
        if (QLibraryInfo::isDebugBuild())
            QSKIP("The debug WebEngine stops on its own checks here; run this test in a release build.");
        QList<QKeySequence> keys = appKeys();
        // Not paste or copy: the debug WebEngine stops on them on the offscreen
        // platform, which has no clipboard of its own.
        const QList<QKeySequence> editing = {QKeySequence(Qt::CTRL | Qt::Key_B), QKeySequence(QKeySequence::SelectAll),
                                             QKeySequence(QKeySequence::Undo)};
        keys << editing;
        Window window(keys);
        window.widget.resize(400, 200);
        auto view = makePage(&window.widget);
        QVERIFY(view);
        QList<QKeySequence> pageKeys;
        PageKeys::install(view.get(), [&pageKeys] { return pageKeys; });
        window.widget.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window.widget));
        QWidget *proxy = view->focusProxy();
        QVERIFY(proxy);
        proxy->setFocus();
        QTRY_VERIFY(proxy->hasFocus());
        QCOMPARE(runJavaScript(view->page(), QStringLiteral("document.getElementById('field').focus();"
                                                            "document.activeElement.id")),
                 QStringLiteral("field"));

        // Every app shortcut runs, also with the focus in the page's field.
        QStringList missed;
        for (const QKeySequence &key : appKeys()) {
            const QString name = key.toString(QKeySequence::PortableText);
            type(proxy, key[0]);
            if (window.ran.value(name) != 1)
                missed << name;
        }
        QCOMPARE(missed, QStringList());
        // Editing keys stay with the page's text field: WebEngine takes them.
        runJavaScript(view->page(), QStringLiteral("window.keys = []"));
        for (const QKeySequence &key : editing) {
            const QString name = key.toString(QKeySequence::PortableText);
            type(proxy, key[0]);
            QVERIFY2(window.ran.value(name) == 0, qPrintable(name + QStringLiteral(" ran")));
        }
        QTRY_COMPARE_WITH_TIMEOUT(runJavaScript(view->page(), QStringLiteral("window.keys.join(',')")),
                                  QStringLiteral("Ctrl+b,Ctrl+a,Ctrl+z"), kTimeout);

        // Outside a text field, Ctrl+B runs the app's shortcut again.
        runJavaScript(view->page(), QStringLiteral("document.activeElement.blur()"));
        type(proxy, Qt::CTRL | Qt::Key_B);
        QCOMPARE(window.ran.value(QStringLiteral("Ctrl+B")), 1);

        // A key given to the page reaches it, and its shortcut does not run.
        pageKeys = {QKeySequence(Qt::CTRL | Qt::Key_K)};
        runJavaScript(view->page(), QStringLiteral("window.keys = []"));
        type(proxy, Qt::CTRL | Qt::Key_K);
        QCOMPARE(window.ran.value(QStringLiteral("Ctrl+K")), 1);
        QTRY_COMPARE_WITH_TIMEOUT(runJavaScript(view->page(), QStringLiteral("window.keys.join(',')")),
                                  QStringLiteral("Ctrl+k"), kTimeout);
    }
};

QTEST_MAIN(TestPageKeys)
#include "tst_page_keys.moc"
