#include "core/Language.h"

#include <QTest>

using namespace mirante;

class TestLanguage : public QObject
{
    Q_OBJECT

private slots:
    void choose_data()
    {
        QTest::addColumn<QString>("setting");
        QTest::addColumn<QStringList>("system");
        QTest::addColumn<QString>("chosen");

        const QStringList brazil{QStringLiteral("pt-BR"), QStringLiteral("en-US")};
        const QStringList english{QStringLiteral("en-GB"), QStringLiteral("pt-BR")};
        QTest::newRow("system, Brazilian") << QStringLiteral("system") << brazil << QStringLiteral("pt_BR");
        QTest::newRow("system, Portugal") << QStringLiteral("system") << QStringList{QStringLiteral("pt-PT")}
                                          << QStringLiteral("pt_BR");
        QTest::newRow("system, plain pt") << QStringLiteral("system") << QStringList{QStringLiteral("pt")}
                                          << QStringLiteral("pt_BR");
        QTest::newRow("system, underscore") << QStringLiteral("system") << QStringList{QStringLiteral("pt_BR")}
                                            << QStringLiteral("pt_BR");
        QTest::newRow("system, English first") << QStringLiteral("system") << english << QStringLiteral("en");
        QTest::newRow("system, other language") << QStringLiteral("system") << QStringList{QStringLiteral("ptx")}
                                                << QStringLiteral("en");
        QTest::newRow("system, none") << QStringLiteral("system") << QStringList() << QStringLiteral("en");
        QTest::newRow("English on a Brazilian system") << QStringLiteral("en") << brazil << QStringLiteral("en");
        QTest::newRow("Portuguese on an English system") << QStringLiteral("pt_BR") << english
                                                         << QStringLiteral("pt_BR");
        QTest::newRow("unknown setting") << QStringLiteral("fr") << brazil << QStringLiteral("pt_BR");
    }

    void choose()
    {
        QFETCH(QString, setting);
        QFETCH(QStringList, system);
        QFETCH(QString, chosen);
        QCOMPARE(language::choose(setting, system), chosen);
    }

    void locale()
    {
        QCOMPARE(language::locale(QStringLiteral("pt_BR")).language(), QLocale::Portuguese);
        QCOMPARE(language::locale(QStringLiteral("pt_BR")).territory(), QLocale::Brazil);
        QCOMPARE(language::locale(QStringLiteral("en")).language(), QLocale::English);
    }
};

QTEST_GUILESS_MAIN(TestLanguage)
#include "tst_language.moc"
