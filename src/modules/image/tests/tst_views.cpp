// The Image view, with a fake context and images made in a temporary folder.
// Nothing opens for real: a handler stands in for the external browser. See
// README.md#view.
#include "../ImageView.h"

#include <mirante/ItemContext.h>

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonObject>
#include <QLabel>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace image = mirante::modules::image;

namespace {

class FakeContext : public mirante::ItemContext
{
public:
    QJsonObject itemConfig;
    QString filesDir;
    bool makesMessages = true;
    QStringList messages;

    QString itemName() const override { return QStringLiteral("Image"); }
    QJsonObject config() const override { return itemConfig; }
    QJsonValue projectSetting(const QString &) const override { return {}; }
    QJsonObject moduleSettings(const QString &) const override { return {}; }
    QString folderPath(const QString &) const override { return {}; }
    QWidget *createWebView(const QUrl &, QWidget *) override { return nullptr; }
    QWebEngineProfile *profile() override { return nullptr; }
    QWidget *createMessageView(const QString &text, QWidget *parent) override
    {
        messages << text;
        return makesMessages ? new QLabel(text, parent) : nullptr;
    }
    void updateConfig(const QJsonObject &) override {}

    // As the core does: the path of a file that exists, else empty.
    QString filePath(const QString &fileName) const override
    {
        const QString path = QDir(filesDir).filePath(fileName);
        return QFileInfo::exists(path) ? path : QString();
    }
};

// Stands in for the external browser.
class UrlCatcher : public QObject
{
    Q_OBJECT

public:
    QList<QUrl> urls;

public slots:
    void openUrl(const QUrl &url) { urls << url; }
};

// Catches every web address that would open, until it goes.
class BrowserGuard
{
public:
    BrowserGuard()
    {
        QDesktopServices::setUrlHandler(QStringLiteral("http"), &catcher, "openUrl");
        QDesktopServices::setUrlHandler(QStringLiteral("https"), &catcher, "openUrl");
    }
    ~BrowserGuard()
    {
        QDesktopServices::unsetUrlHandler(QStringLiteral("http"));
        QDesktopServices::unsetUrlHandler(QStringLiteral("https"));
    }
    UrlCatcher catcher;
};

bool isRed(QRgb pixel)
{
    return qRed(pixel) > 200 && qGreen(pixel) < 60 && qBlue(pixel) < 60;
}

// The rectangle of red pixels in a picture of the view.
QRect redArea(const QImage &picture)
{
    QRect area;
    for (int y = 0; y < picture.height(); ++y) {
        for (int x = 0; x < picture.width(); ++x) {
            if (isRed(picture.pixel(x, y)))
                area |= QRect(x, y, 1, 1);
        }
    }
    return area;
}

} // namespace

class TestViews : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    std::unique_ptr<FakeContext> m_owned;
    FakeContext &m_context() { return *m_owned; }

    // A red image of the given size, saved as PNG, and set as the item's file.
    void useImage(const QSize &size)
    {
        QImage picture(size, QImage::Format_RGB32);
        picture.fill(Qt::red);
        QVERIFY(picture.save(m_dir.filePath(QStringLiteral("picture.png"))));
        m_context().itemConfig.insert(QStringLiteral("file"), QStringLiteral("picture.png"));
    }

    QWidget *pictureOf(QWidget *view) { return view->findChild<QWidget *>(QStringLiteral("image")); }
    QLabel *memoOf(QWidget *view) { return view->findChild<QLabel *>(QStringLiteral("memo")); }

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_owned = std::make_unique<FakeContext>();
        m_owned->filesDir = m_dir.path();
    }

    void keepsTheAspectRatio_data()
    {
        QTest::addColumn<QSize>("viewSize");
        QTest::addColumn<QSize>("shown");
        QTest::newRow("square") << QSize(300, 300) << QSize(300, 150);
        QTest::newRow("tall") << QSize(100, 400) << QSize(100, 50);
        QTest::newRow("wide") << QSize(500, 120) << QSize(240, 120);
        QTest::newRow("larger than the image") << QSize(800, 800) << QSize(800, 400);
    }

    void keepsTheAspectRatio()
    {
        QFETCH(QSize, viewSize);
        QFETCH(QSize, shown);
        useImage(QSize(200, 100));
        std::unique_ptr<QWidget> view(image::createImageView(m_context(), nullptr));
        QVERIFY(pictureOf(view.get()));
        view->resize(viewSize);
        view->show();
        QVERIFY(QTest::qWaitForWindowExposed(view.get()));

        const QImage picture = view->grab().toImage().scaled(viewSize);
        const QRect area = redArea(picture);
        QVERIFY(qAbs(area.width() - shown.width()) <= 2);
        QVERIFY(qAbs(area.height() - shown.height()) <= 2);
        // Centred.
        QVERIFY(qAbs(area.center().x() - picture.rect().center().x()) <= 2);
        QVERIFY(qAbs(area.center().y() - picture.rect().center().y()) <= 2);
    }

    void showsTheMemo()
    {
        useImage(QSize(20, 20));
        m_context().itemConfig.insert(QStringLiteral("memo"), QStringLiteral("Line one\nLine <b>two</b>"));
        std::unique_ptr<QWidget> view(image::createImageView(m_context(), nullptr));
        QLabel *memo = memoOf(view.get());
        QVERIFY(memo);
        QVERIFY(!memo->isHidden());
        QCOMPARE(memo->text(), QStringLiteral("Line one\nLine <b>two</b>"));
        QCOMPARE(memo->textFormat(), Qt::PlainText);
        QVERIFY(memo->wordWrap());
        QVERIFY(memo->textInteractionFlags() & Qt::TextSelectableByMouse);
    }

    void hidesAnEmptyMemo()
    {
        useImage(QSize(20, 20));
        std::unique_ptr<QWidget> view(image::createImageView(m_context(), nullptr));
        QVERIFY(memoOf(view.get())->isHidden());

        m_context().itemConfig.insert(QStringLiteral("memo"), QStringLiteral("  \n"));
        view.reset(image::createImageView(m_context(), nullptr));
        QVERIFY(memoOf(view.get())->isHidden());
    }

    void clickOpensTheLink()
    {
        BrowserGuard browser;
        useImage(QSize(40, 40));
        m_context().itemConfig.insert(QStringLiteral("link"), QStringLiteral("https://example.com/diagram"));
        std::unique_ptr<QWidget> view(image::createImageView(m_context(), nullptr));
        view->resize(100, 100);
        view->show();
        QVERIFY(QTest::qWaitForWindowExposed(view.get()));

        QWidget *picture = pictureOf(view.get());
        QCOMPARE(picture->cursor().shape(), Qt::PointingHandCursor);
        QCOMPARE(picture->toolTip(), QStringLiteral("https://example.com/diagram"));
        QTest::mouseClick(picture, Qt::LeftButton);
        QCOMPARE(browser.catcher.urls, QList<QUrl>{QUrl(QStringLiteral("https://example.com/diagram"))});
    }

    void clickWithoutALinkOpensNothing()
    {
        BrowserGuard browser;
        useImage(QSize(40, 40));
        std::unique_ptr<QWidget> view(image::createImageView(m_context(), nullptr));
        view->resize(100, 100);
        view->show();
        QVERIFY(QTest::qWaitForWindowExposed(view.get()));

        QWidget *picture = pictureOf(view.get());
        QVERIFY(!picture->testAttribute(Qt::WA_SetCursor));
        QVERIFY(picture->toolTip().isEmpty());
        QTest::mouseClick(picture, Qt::LeftButton);
        QVERIFY(browser.catcher.urls.isEmpty());
    }

    // Only web addresses open; anything else counts as no link.
    void otherSchemesAreNoLink()
    {
        BrowserGuard browser;
        useImage(QSize(40, 40));
        m_context().itemConfig.insert(QStringLiteral("link"), QStringLiteral("file:///C:/Windows/notepad.exe"));
        std::unique_ptr<QWidget> view(image::createImageView(m_context(), nullptr));
        QVERIFY(!pictureOf(view.get())->testAttribute(Qt::WA_SetCursor));
        QVERIFY(pictureOf(view.get())->toolTip().isEmpty());
    }

    void missingFile()
    {
        m_context().itemConfig.insert(QStringLiteral("file"), QStringLiteral("gone.png"));
        std::unique_ptr<QWidget> view(image::createImageView(m_context(), nullptr));
        QCOMPARE(m_context().messages, QStringList{QStringLiteral("The image file is missing.")});
        QVERIFY(!pictureOf(view.get()));

        // Without the core's message view, a label of its own.
        m_context().makesMessages = false;
        m_context().itemConfig.remove(QStringLiteral("file"));
        view.reset(image::createImageView(m_context(), nullptr));
        auto *label = qobject_cast<QLabel *>(view.get());
        QVERIFY(label);
        QCOMPARE(label->text(), QStringLiteral("The image file is missing."));
    }

    void unreadableFile()
    {
        QFile broken(m_dir.filePath(QStringLiteral("broken.png")));
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("not an image");
        broken.close();
        m_context().itemConfig.insert(QStringLiteral("file"), QStringLiteral("broken.png"));
        std::unique_ptr<QWidget> view(image::createImageView(m_context(), nullptr));
        QCOMPARE(m_context().messages, QStringList{QStringLiteral("The image file cannot be read.")});
    }
};

QTEST_MAIN(TestViews)
#include "tst_views.moc"
