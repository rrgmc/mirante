// Tests validate() and defaultConfig() of FieldValidation.
#include "items/FieldValidation.h"

#include <QJsonArray>
#include <QTest>

using namespace mirante;

namespace {

Field field(const QString &key, Field::Type type, bool required = false)
{
    Field f;
    f.key = key;
    f.label = key.at(0).toUpper() + key.mid(1);
    f.type = type;
    f.required = required;
    return f;
}

Field choice(bool required = false)
{
    Field f = field(QStringLiteral("state"), Field::Type::Choice, required);
    f.choices = {QStringLiteral("opened"), QStringLiteral("merged")};
    f.choiceLabels = {QStringLiteral("Opened"), QStringLiteral("Merged")};
    return f;
}

// The keys of the problems, in order.
QStringList keys(const std::vector<FieldProblem> &problems)
{
    QStringList result;
    for (const FieldProblem &problem : problems)
        result << problem.key;
    return result;
}

} // namespace

Q_DECLARE_METATYPE(mirante::Field)

class TestFieldValidation : public QObject
{
    Q_OBJECT

private slots:
    void values_data()
    {
        QTest::addColumn<Field>("field");
        QTest::addColumn<QJsonValue>("value");
        QTest::addColumn<bool>("valid");

        const Field text = field(QStringLiteral("name"), Field::Type::Text);
        const Field requiredText = field(QStringLiteral("name"), Field::Type::Text, true);
        const Field url = field(QStringLiteral("url"), Field::Type::Url);
        const Field number = field(QStringLiteral("number"), Field::Type::Integer);
        const Field requiredNumber = field(QStringLiteral("number"), Field::Type::Integer, true);
        const Field flag = field(QStringLiteral("flag"), Field::Type::Bool);
        const Field folder = field(QStringLiteral("folder"), Field::Type::Folder, true);
        const Field image = field(QStringLiteral("file"), Field::Type::Image);
        const Field notes = field(QStringLiteral("notes"), Field::Type::MultiLineText, true);
        const QJsonValue missing(QJsonValue::Undefined);

        QTest::newRow("text") << text << QJsonValue(QStringLiteral("a")) << true;
        QTest::newRow("optional text missing") << text << missing << true;
        QTest::newRow("optional text empty") << text << QJsonValue(QString()) << true;
        QTest::newRow("text not a string") << text << QJsonValue(3) << false;
        QTest::newRow("required text") << requiredText << QJsonValue(QStringLiteral("a")) << true;
        QTest::newRow("required text missing") << requiredText << missing << false;
        QTest::newRow("required text null") << requiredText << QJsonValue(QJsonValue::Null) << false;
        QTest::newRow("required text blank") << requiredText << QJsonValue(QStringLiteral("  ")) << false;
        QTest::newRow("required multi-line") << notes << QJsonValue(QStringLiteral("a\nb")) << true;
        QTest::newRow("required multi-line empty") << notes << QJsonValue(QString()) << false;

        QTest::newRow("https") << url << QJsonValue(QStringLiteral("https://example.com/a?b=c")) << true;
        QTest::newRow("http") << url << QJsonValue(QStringLiteral("http://localhost:8080")) << true;
        QTest::newRow("upper-case scheme") << url << QJsonValue(QStringLiteral("HTTPS://example.com")) << true;
        QTest::newRow("ftp") << url << QJsonValue(QStringLiteral("ftp://example.com")) << false;
        QTest::newRow("no scheme") << url << QJsonValue(QStringLiteral("example.com")) << false;
        QTest::newRow("no host") << url << QJsonValue(QStringLiteral("https://")) << false;
        QTest::newRow("url not a string") << url << QJsonValue(true) << false;

        QTest::newRow("integer") << number << QJsonValue(42) << true;
        QTest::newRow("negative integer") << number << QJsonValue(-7) << true;
        QTest::newRow("fraction") << number << QJsonValue(1.5) << false;
        QTest::newRow("integer as string") << number << QJsonValue(QStringLiteral("42")) << false;
        QTest::newRow("optional integer missing") << number << missing << true;
        QTest::newRow("required integer missing") << requiredNumber << missing << false;
        QTest::newRow("required integer zero") << requiredNumber << QJsonValue(0) << true;

        QTest::newRow("bool") << flag << QJsonValue(false) << true;
        QTest::newRow("bool as string") << flag << QJsonValue(QStringLiteral("true")) << false;

        QTest::newRow("choice") << choice() << QJsonValue(QStringLiteral("merged")) << true;
        QTest::newRow("choice by label") << choice() << QJsonValue(QStringLiteral("Merged")) << false;
        QTest::newRow("choice not listed") << choice() << QJsonValue(QStringLiteral("closed")) << false;
        QTest::newRow("optional choice missing") << choice() << missing << true;
        QTest::newRow("required choice missing") << choice(true) << missing << false;

        QTest::newRow("folder") << folder << QJsonValue(QStringLiteral("f1")) << true;
        QTest::newRow("folder missing") << folder << missing << false;
        QTest::newRow("image") << image << QJsonValue(QStringLiteral("a.png")) << true;
        QTest::newRow("image not a string") << image << QJsonValue(QJsonArray()) << false;
    }

    void values()
    {
        QFETCH(Field, field);
        QFETCH(QJsonValue, value);
        QFETCH(bool, valid);

        QJsonObject config;
        if (!value.isUndefined())
            config.insert(field.key, value);
        const std::vector<FieldProblem> problems = validate({field}, config);
        QCOMPARE(problems.empty(), valid);
        if (!valid) {
            QCOMPARE(problems.size(), std::size_t(1));
            QCOMPARE(problems.front().key, field.key);
            QVERIFY(problems.front().message.contains(field.label));
        }
    }

    void requiredMessageNamesTheLabel()
    {
        Field f = field(QStringLiteral("path"), Field::Type::Text, true);
        f.label = QStringLiteral("Project path");
        const std::vector<FieldProblem> problems = validate({f}, {});
        QCOMPARE(problems.size(), std::size_t(1));
        QCOMPARE(problems.front().message, QStringLiteral("Project path is required"));
    }

    void onlyFieldKeysAreChecked()
    {
        const std::vector<Field> fields = {field(QStringLiteral("a"), Field::Type::Integer),
                                           field(QStringLiteral("b"), Field::Type::Text, true),
                                           field(QStringLiteral("c"), Field::Type::Url)};
        const QJsonObject config{{QStringLiteral("a"), QStringLiteral("x")},
                                 {QStringLiteral("c"), QStringLiteral("mailto:me")},
                                 {QStringLiteral("other"), QJsonArray{1, 2}}};
        QCOMPARE(keys(validate(fields, config)),
                 (QStringList{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));
        QVERIFY(validate({}, config).empty());
    }

    void defaults()
    {
        Field name = field(QStringLiteral("name"), Field::Type::Text);
        name.defaultValue = QStringLiteral("Main");
        Field number = field(QStringLiteral("number"), Field::Type::Integer);
        number.defaultValue = 10;
        Field flag = field(QStringLiteral("flag"), Field::Type::Bool);
        flag.defaultValue = false;
        Field state = choice();
        state.defaultValue = QStringLiteral("opened");
        const Field none = field(QStringLiteral("none"), Field::Type::Url);

        const std::vector<Field> fields = {name, number, flag, state, none};
        const QJsonObject expected{{QStringLiteral("name"), QStringLiteral("Main")},
                                   {QStringLiteral("number"), 10},
                                   {QStringLiteral("flag"), false},
                                   {QStringLiteral("state"), QStringLiteral("opened")}};
        QCOMPARE(defaultConfig(fields), expected);
        QVERIFY(validate(fields, defaultConfig(fields)).empty());
        QVERIFY(defaultConfig({}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestFieldValidation)
#include "tst_field_validation.moc"
