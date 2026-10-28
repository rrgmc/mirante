// Checks that the icons of the app's own buttons are compiled in and render.
#include <QIcon>
#include <QTest>

class TestAppIcons : public QObject
{
    Q_OBJECT

private slots:
    void iconsRender_data()
    {
        QTest::addColumn<QString>("name");
        for (const char *name : {"collapse-all", "dashboard", "dashboards", "expand-all", "lock-closed", "lock-open", "open-alone", "pin"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }

    void iconsRender()
    {
        QFETCH(QString, name);
        const QIcon icon(QStringLiteral(":/icons/%1.svg").arg(name));
        QVERIFY(!icon.pixmap(16, 16).isNull());
    }
};

QTEST_MAIN(TestAppIcons)
#include "tst_app_icons.moc"
