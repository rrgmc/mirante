// Tests PageFreezer: the pages of hidden windows freeze and come back, and a
// page deleted while frozen is forgotten.
#include "web/PageFreezer.h"

#include <QApplication>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QWidget>

#include <memory>

using namespace mirante;

namespace {

// Starting Chromium for the first page takes a while.
constexpr int kTimeout = 20000;

using State = QWebEnginePage::LifecycleState;

} // namespace

class TestPageFreezer : public QObject
{
    Q_OBJECT

private:
    // Off the record, so the test writes nothing to disk.
    std::unique_ptr<QWebEngineProfile> profile;

    // A shown window with a web view that has loaded a page.
    std::unique_ptr<QWidget> makeWindow(QWebEngineView **viewOut)
    {
        auto window = std::make_unique<QWidget>();
        auto *layout = new QVBoxLayout(window.get());
        auto *view = new QWebEngineView(window.get());
        view->setPage(new QWebEnginePage(profile.get(), view));
        layout->addWidget(view);
        window->resize(400, 300);
        window->show();
        QSignalSpy loaded(view, &QWebEngineView::loadFinished);
        view->setHtml(QStringLiteral("<p>Hello</p>"));
        if (loaded.isEmpty())
            loaded.wait(kTimeout);
        *viewOut = view;
        return window;
    }

private slots:
    void initTestCase() { profile = std::make_unique<QWebEngineProfile>(); }

    void cleanupTestCase()
    {
        // Pages release their resources later; let them go before the profile.
        QTest::qWait(100);
        profile.reset();
    }

    void freezesAndThaws()
    {
        QWebEngineView *view = nullptr;
        const std::unique_ptr<QWidget> window = makeWindow(&view);
        QWebEnginePage *page = view->page();
        QCOMPARE(page->lifecycleState(), State::Active);

        window->hide();
        PageFreezer freezer;
        freezer.freeze({window.get()});
        QTRY_COMPARE_WITH_TIMEOUT(page->lifecycleState(), State::Frozen, kTimeout);

        freezer.thaw();
        QCOMPARE(page->lifecycleState(), State::Active);
        window->show();
        QCOMPARE(page->lifecycleState(), State::Active);
    }

    // A thaw without a freeze, and a second freeze, change nothing.
    void repeatedCalls()
    {
        QWebEngineView *view = nullptr;
        const std::unique_ptr<QWidget> window = makeWindow(&view);
        QWebEnginePage *page = view->page();
        PageFreezer freezer;
        freezer.thaw();
        QCOMPARE(page->lifecycleState(), State::Active);

        window->hide();
        freezer.freeze({window.get()});
        freezer.freeze({window.get(), nullptr});
        QTRY_COMPARE_WITH_TIMEOUT(page->lifecycleState(), State::Frozen, kTimeout);
        freezer.thaw();
        QCOMPARE(page->lifecycleState(), State::Active);
    }

    void deletedPageIsForgotten()
    {
        QWebEngineView *view = nullptr;
        std::unique_ptr<QWidget> window = makeWindow(&view);
        window->hide();
        PageFreezer freezer;
        freezer.freeze({window.get()});
        window.reset();
        freezer.thaw();
    }
};

int main(int argc, char *argv[])
{
    // Qt WebEngine needs shared OpenGL contexts, set before the application.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    // Without a display, Chromium's GPU process can fail a check when a view
    // goes away. Pages render in software instead.
    if (qEnvironmentVariableIsEmpty("QTWEBENGINE_CHROMIUM_FLAGS"))
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-gpu");
    QApplication app(argc, argv);
    TestPageFreezer test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_page_freezer.moc"
