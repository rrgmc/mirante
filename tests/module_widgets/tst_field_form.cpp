// Tests FieldForm: one editor per field type, reading back an unchanged
// configuration, keeping keys it does not show, edits, problems, images,
// pasted or dropped from another app, and the state of Secret fields.
#include <mirante/widgets/FieldForm.h>
#include <mirante/widgets/ImagePasteDialog.h>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QImage>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QGroupBox>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>

using namespace mirante;
using mirante::widgets::FieldForm;
using mirante::widgets::ImagePasteDialog;

namespace {

Field field(const QString &key, const QString &label, Field::Type type, bool required = false)
{
    Field f;
    f.key = key;
    f.label = label;
    f.type = type;
    f.required = required;
    return f;
}

// One field of every type.
std::vector<Field> allFields()
{
    Field state = field(QStringLiteral("state"), QStringLiteral("State"), Field::Type::Choice);
    state.choices = {QStringLiteral("opened"), QStringLiteral("merged")};
    state.choiceLabels = {QStringLiteral("Opened"), QStringLiteral("Merged")};
    return {
        field(QStringLiteral("name"), QStringLiteral("Name"), Field::Type::Text, true),
        field(QStringLiteral("notes"), QStringLiteral("Notes"), Field::Type::MultiLineText),
        field(QStringLiteral("url"), QStringLiteral("Address"), Field::Type::Url),
        field(QStringLiteral("number"), QStringLiteral("Number"), Field::Type::Integer),
        field(QStringLiteral("draft"), QStringLiteral("Draft"), Field::Type::Bool),
        state,
        field(QStringLiteral("folder"), QStringLiteral("Folder"), Field::Type::Folder),
        field(QStringLiteral("file"), QStringLiteral("Image"), Field::Type::Image),
    };
}

// A line that upper-cases what it stores, and refuses "bad".
class ShoutEditor : public mirante::widgets::FieldEditor
{
public:
    explicit ShoutEditor(QWidget *parent = nullptr)
        : FieldEditor(parent)
        , line(new QLineEdit(this))
    {
        connect(line, &QLineEdit::textEdited, this, &FieldEditor::changed);
    }
    void setValue(const QJsonValue &value) override { line->setText(value.toString().toLower()); }
    QJsonValue value() const override
    {
        return line->text().isEmpty() ? QJsonValue(QJsonValue::Undefined) : QJsonValue(line->text().toUpper());
    }
    QStringList problems() const override
    {
        return line->text() == QLatin1String("bad") ? QStringList{QStringLiteral("Bad.")} : QStringList();
    }
    QLineEdit *line;
};

EditorContext context(QStringList *saved = nullptr)
{
    EditorContext c;
    c.folders = {{QStringLiteral("f1"), QStringLiteral("backend")}, {QStringLiteral("f2"), QStringLiteral("web")}};
    c.saveImage = [saved](const QImage &) {
        const QString name = QStringLiteral("pasted.png");
        if (saved)
            *saved << name;
        return name;
    };
    return c;
}

// Sends a drag over target and drops it there, as a drag from another app
// does. Returns the action of an accepted drop, or IgnoreAction for a drag
// refused at any step.
Qt::DropAction dragAndDrop(QWidget *target, const QMimeData *mime)
{
    const Qt::DropActions actions = Qt::CopyAction | Qt::MoveAction;
    const QPoint pos = target->rect().center();
    QDragEnterEvent enter(pos, actions, mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &enter);
    if (!enter.isAccepted())
        return Qt::IgnoreAction;
    QDragMoveEvent move(pos, actions, mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &move);
    if (!move.isAccepted()) {
        QDragLeaveEvent leave;
        QApplication::sendEvent(target, &leave);
        return Qt::IgnoreAction;
    }
    QDropEvent drop(QPointF(pos), actions, mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &drop);
    return drop.isAccepted() ? drop.dropAction() : Qt::IgnoreAction;
}

QJsonObject fullConfig()
{
    return {
        {QStringLiteral("name"), QStringLiteral("Login fix")},
        {QStringLiteral("notes"), QStringLiteral("line one\nline two")},
        {QStringLiteral("url"), QStringLiteral("https://example.com/mr/1")},
        {QStringLiteral("number"), 123456789},
        {QStringLiteral("draft"), true},
        {QStringLiteral("state"), QStringLiteral("merged")},
        {QStringLiteral("folder"), QStringLiteral("f2")},
        {QStringLiteral("file"), QStringLiteral("a.png")},
        {QStringLiteral("extra"), QJsonArray{1, QStringLiteral("two")}},
    };
}

} // namespace

class TestFieldForm : public QObject
{
    Q_OBJECT

private slots:
    void editorPerType()
    {
        FieldForm form(allFields(), context());
        QVERIFY(qobject_cast<QLineEdit *>(form.editorFor(QStringLiteral("name"))));
        QVERIFY(qobject_cast<QPlainTextEdit *>(form.editorFor(QStringLiteral("notes"))));
        QVERIFY(qobject_cast<QLineEdit *>(form.editorFor(QStringLiteral("url"))));
        QVERIFY(qobject_cast<QSpinBox *>(form.editorFor(QStringLiteral("number"))));
        QVERIFY(qobject_cast<QCheckBox *>(form.editorFor(QStringLiteral("draft"))));
        QVERIFY(qobject_cast<QComboBox *>(form.editorFor(QStringLiteral("state"))));
        QVERIFY(qobject_cast<QComboBox *>(form.editorFor(QStringLiteral("folder"))));
        QWidget *image = form.editorFor(QStringLiteral("file"));
        QVERIFY(image);
        QVERIFY(image->findChild<QPushButton *>(QStringLiteral("pasteImage")));
        QVERIFY(image->findChild<QPushButton *>(QStringLiteral("chooseImageFile")));
        QVERIFY(!form.editorFor(QStringLiteral("extra")));
    }

    void showsValues()
    {
        FieldForm form(allFields(), context());
        form.setConfig(fullConfig());
        QCOMPARE(qobject_cast<QLineEdit *>(form.editorFor(QStringLiteral("name")))->text(),
                 QStringLiteral("Login fix"));
        QCOMPARE(qobject_cast<QSpinBox *>(form.editorFor(QStringLiteral("number")))->value(), 123456789);
        QVERIFY(qobject_cast<QCheckBox *>(form.editorFor(QStringLiteral("draft")))->isChecked());
        auto *state = qobject_cast<QComboBox *>(form.editorFor(QStringLiteral("state")));
        QCOMPARE(state->currentText(), QStringLiteral("Merged"));
        auto *folder = qobject_cast<QComboBox *>(form.editorFor(QStringLiteral("folder")));
        QCOMPARE(folder->currentText(), QStringLiteral("web"));
        QCOMPARE(form.editorFor(QStringLiteral("file"))->findChild<QLabel *>(QStringLiteral("imageName"))->text(),
                 QStringLiteral("a.png"));
    }

    void readsBackUnchanged_data()
    {
        QTest::addColumn<QJsonObject>("config");
        QTest::newRow("full") << fullConfig();
        QTest::newRow("empty") << QJsonObject();
        QTest::newRow("extra only") << QJsonObject{{QStringLiteral("other"), 1}};
        QTest::newRow("empty strings")
            << QJsonObject{{QStringLiteral("name"), QString()}, {QStringLiteral("url"), QString()}};
        // Values the editors cannot show are kept as they are.
        QTest::newRow("invalid values")
            << QJsonObject{{QStringLiteral("number"), QStringLiteral("many")},
                           {QStringLiteral("draft"), QStringLiteral("yes")},
                           {QStringLiteral("state"), QStringLiteral("closed")},
                           {QStringLiteral("folder"), QStringLiteral("gone")},
                           {QStringLiteral("notes"), 3}};
    }

    void readsBackUnchanged()
    {
        QFETCH(QJsonObject, config);
        FieldForm form(allFields(), context());
        QSignalSpy changed(&form, &FieldForm::changed);
        form.setConfig(config);
        QCOMPARE(form.config(), config);
        QCOMPARE(changed.count(), 0);
    }

    void editsKeepOtherKeys()
    {
        FieldForm form(allFields(), context());
        QSignalSpy changed(&form, &FieldForm::changed);
        form.setConfig(fullConfig());

        qobject_cast<QLineEdit *>(form.editorFor(QStringLiteral("name")))->setText(QStringLiteral("Renamed"));
        qobject_cast<QSpinBox *>(form.editorFor(QStringLiteral("number")))->setValue(-5);
        qobject_cast<QCheckBox *>(form.editorFor(QStringLiteral("draft")))->setChecked(false);
        auto *state = qobject_cast<QComboBox *>(form.editorFor(QStringLiteral("state")));
        state->setCurrentIndex(state->findData(QStringLiteral("opened")));
        auto *folder = qobject_cast<QComboBox *>(form.editorFor(QStringLiteral("folder")));
        folder->setCurrentIndex(folder->findData(QStringLiteral("f1")));
        qobject_cast<QPlainTextEdit *>(form.editorFor(QStringLiteral("notes")))->clear();
        QVERIFY(changed.count() >= 6);

        QJsonObject expected = fullConfig();
        expected[QStringLiteral("name")] = QStringLiteral("Renamed");
        expected[QStringLiteral("number")] = -5;
        expected[QStringLiteral("draft")] = false;
        expected[QStringLiteral("state")] = QStringLiteral("opened");
        expected[QStringLiteral("folder")] = QStringLiteral("f1");
        expected.remove(QStringLiteral("notes"));
        QCOMPARE(form.config(), expected);

        // setConfig() starts over.
        form.setConfig(fullConfig());
        QCOMPARE(form.config(), fullConfig());
    }

    void newValues()
    {
        FieldForm form(allFields(), context());
        form.setConfig({});
        auto *state = qobject_cast<QComboBox *>(form.editorFor(QStringLiteral("state")));
        QCOMPARE(state->currentText(), QString());
        state->setCurrentIndex(state->findData(QStringLiteral("merged")));
        qobject_cast<QLineEdit *>(form.editorFor(QStringLiteral("url")))->setText(QStringLiteral("https://a.b"));
        QCOMPARE(form.config(), (QJsonObject{{QStringLiteral("state"), QStringLiteral("merged")},
                                            {QStringLiteral("url"), QStringLiteral("https://a.b")}}));
    }

    void problems()
    {
        FieldForm form(allFields(), context());
        form.setConfig({});
        QCOMPARE(form.problems(), QStringList{QStringLiteral("Name is required")});

        auto *name = qobject_cast<QLineEdit *>(form.editorFor(QStringLiteral("name")));
        name->setText(QStringLiteral("x"));
        qobject_cast<QLineEdit *>(form.editorFor(QStringLiteral("url")))->setText(QStringLiteral("example.com"));
        QCOMPARE(form.problems().size(), 1);
        QVERIFY(form.problems().front().startsWith(QStringLiteral("Address")));

        qobject_cast<QLineEdit *>(form.editorFor(QStringLiteral("url")))->setText(QStringLiteral("http://example.com"));
        QVERIFY(form.problems().isEmpty());
    }

    // "Paste" opens the paste dialog. Cancel keeps the image; Paste saves
    // the cropped image.
    void pasteImage()
    {
        QStringList saved;
        EditorContext c = context(&saved);
        QSize savedSize;
        c.saveImage = [&saved, &savedSize](const QImage &image) {
            savedSize = image.size();
            saved << QStringLiteral("pasted.png");
            return QStringLiteral("pasted.png");
        };
        FieldForm form(allFields(), c);
        QSignalSpy changed(&form, &FieldForm::changed);
        form.setConfig(fullConfig());

        QImage image(40, 30, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QApplication::clipboard()->setImage(image);
        if (QApplication::clipboard()->image().isNull())
            QSKIP("The clipboard does not hold images on this platform.");

        QWidget *editor = form.editorFor(QStringLiteral("file"));
        int dialogs = 0;
        form.setExec([&dialogs](QDialog &) {
            ++dialogs;
            return false;
        });
        editor->findChild<QPushButton *>(QStringLiteral("pasteImage"))->click();
        QCOMPARE(dialogs, 1);
        QVERIFY(saved.isEmpty());
        QCOMPARE(changed.count(), 0);

        form.setExec([](QDialog &d) {
            auto *dialog = qobject_cast<ImagePasteDialog *>(&d);
            if (dialog)
                dialog->setCrop(QRect(5, 5, 10, 20));
            return dialog != nullptr;
        });
        editor->findChild<QPushButton *>(QStringLiteral("pasteImage"))->click();
        QCOMPARE(saved, QStringList{QStringLiteral("pasted.png")});
        QCOMPARE(savedSize, QSize(10, 20));
        QCOMPARE(changed.count(), 1);
        QVERIFY(!editor->findChild<QLabel *>(QStringLiteral("imageThumbnail"))->pixmap().isNull());

        QJsonObject expected = fullConfig();
        expected[QStringLiteral("file")] = QStringLiteral("pasted.png");
        QCOMPARE(form.config(), expected);
    }

    // An image dropped from another app on an image field does what Paste
    // does: the paste dialog, then the cropped image is saved. The drop is a
    // copy. A drag without an image is refused.
    void dropImage()
    {
        QStringList saved;
        EditorContext c = context(&saved);
        QSize savedSize;
        c.saveImage = [&saved, &savedSize](const QImage &image) {
            savedSize = image.size();
            saved << QStringLiteral("dropped.png");
            return QStringLiteral("dropped.png");
        };
        FieldForm form(allFields(), c);
        form.setConfig(fullConfig());
        QWidget *editor = form.editorFor(QStringLiteral("file"));
        QVERIFY(editor->acceptDrops());

        QMimeData text;
        text.setText(QStringLiteral("not an image"));
        QCOMPARE(dragAndDrop(editor, &text), Qt::IgnoreAction);

        QImage image(40, 30, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QMimeData mime;
        mime.setImageData(image);
        QStringList dialogs;
        form.setExec([&dialogs](QDialog &d) {
            dialogs << QString::fromLatin1(d.metaObject()->className());
            auto *dialog = qobject_cast<ImagePasteDialog *>(&d);
            if (dialog)
                dialog->setCrop(QRect(5, 5, 10, 20));
            return dialog != nullptr;
        });
        // A drop on the field's button reaches the field.
        QCOMPARE(dragAndDrop(editor->findChild<QPushButton *>(QStringLiteral("pasteImage")), &mime), Qt::CopyAction);
        // The dialog opens once the drop is over.
        QVERIFY(dialogs.isEmpty());
        QTRY_COMPARE(dialogs, QStringList{QStringLiteral("mirante::widgets::ImagePasteDialog")});
        QCOMPARE(saved, QStringList{QStringLiteral("dropped.png")});
        QCOMPARE(savedSize, QSize(10, 20));
        QCOMPARE(form.config().value(QStringLiteral("file")).toString(), QStringLiteral("dropped.png"));
    }

    // Without saveImage, the buttons are disabled and the field takes no drops.
    void imageButtonsNeedSaveImage()
    {
        FieldForm form(allFields(), EditorContext{});
        QWidget *editor = form.editorFor(QStringLiteral("file"));
        QVERIFY(!editor->findChild<QPushButton *>(QStringLiteral("pasteImage"))->isEnabled());
        QVERIFY(!editor->findChild<QPushButton *>(QStringLiteral("chooseImageFile"))->isEnabled());
        QVERIFY(!editor->acceptDrops());
        QImage image(4, 3, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QMimeData mime;
        mime.setImageData(image);
        QCOMPARE(dragAndDrop(editor, &mime), Qt::IgnoreAction);
    }

    void requiredChoiceHasNoEmptyEntry()
    {
        Field state = field(QStringLiteral("state"), QStringLiteral("State"), Field::Type::Choice, true);
        state.choices = {QStringLiteral("a"), QStringLiteral("b")};
        FieldForm form({state}, context());
        form.setConfig({{QStringLiteral("state"), QStringLiteral("b")}});
        auto *combo = qobject_cast<QComboBox *>(form.editorFor(QStringLiteral("state")));
        QCOMPARE(combo->count(), 2);
        // Labels fall back to the stored values.
        QCOMPARE(combo->currentText(), QStringLiteral("b"));
    }

    // A text list is edited one entry per line. Entries are trimmed, and
    // blank lines dropped; no entries remove the key.
    void textList()
    {
        FieldForm form({field(QStringLiteral("paths"), QStringLiteral("Paths"), Field::Type::TextList)}, context());
        auto *text = qobject_cast<QPlainTextEdit *>(form.editorFor(QStringLiteral("paths")));
        QVERIFY(text);

        const QJsonObject config{{QStringLiteral("paths"), QJsonArray{QStringLiteral("a/b"), QStringLiteral("c")}},
                                 {QStringLiteral("other"), 1}};
        form.setConfig(config);
        QCOMPARE(text->toPlainText(), QStringLiteral("a/b\nc"));
        QCOMPARE(form.config(), config);

        text->setPlainText(QStringLiteral("  x/y \n\n z\n"));
        QCOMPARE(form.config().value(QStringLiteral("paths")).toArray(),
                 (QJsonArray{QStringLiteral("x/y"), QStringLiteral("z")}));
        QCOMPARE(form.config().value(QStringLiteral("other")).toInt(), 1);
        QVERIFY(form.problems().isEmpty());

        text->setPlainText(QStringLiteral("\n  \n"));
        QVERIFY(!form.config().contains(QStringLiteral("paths")));
    }

    // A text field with suggestions is an editable combo box: it offers them,
    // and still takes any other text.
    void textSuggestions()
    {
        Field project = field(QStringLiteral("project"), QStringLiteral("Project"), Field::Type::Text);
        project.suggestions = {QStringLiteral("acme/backend"), QStringLiteral("acme/web")};
        project.placeholder = QStringLiteral("group/project");
        FieldForm form({project}, context());
        auto *combo = qobject_cast<QComboBox *>(form.editorFor(QStringLiteral("project")));
        QVERIFY(combo);
        QVERIFY(combo->isEditable());
        QCOMPARE(combo->count(), 2);
        QCOMPARE(combo->itemText(1), QStringLiteral("acme/web"));
        QCOMPARE(combo->lineEdit()->placeholderText(), QStringLiteral("group/project"));

        // Empty and unedited: nothing is written.
        QCOMPARE(combo->currentText(), QString());
        QCOMPARE(form.config(), QJsonObject());

        form.setConfig({{QStringLiteral("project"), QStringLiteral("acme/web")}});
        QCOMPARE(combo->currentIndex(), 1);
        QCOMPARE(combo->currentText(), QStringLiteral("acme/web"));

        QSignalSpy changed(&form, &FieldForm::changed);
        combo->setCurrentIndex(0);
        QCOMPARE(form.config().value(QStringLiteral("project")).toString(), QStringLiteral("acme/backend"));
        QVERIFY(changed.count() >= 1);

        combo->lineEdit()->setText(QStringLiteral("other/project"));
        QCOMPARE(form.config().value(QStringLiteral("project")).toString(), QStringLiteral("other/project"));
        QCOMPARE(combo->count(), 2);

        // A value not in the list is shown as it is.
        form.setConfig({{QStringLiteral("project"), QStringLiteral("solo/repo")}});
        QCOMPARE(combo->currentText(), QStringLiteral("solo/repo"));
        QCOMPARE(form.config().value(QStringLiteral("project")).toString(), QStringLiteral("solo/repo"));
    }

    // A Secret field says whether its token is set on this device, in its
    // placeholder, and never shows the token. A set token has a Remove button.
    void secretState()
    {
        Field token = field(QStringLiteral("token"), QStringLiteral("Token"), Field::Type::Secret);
        token.placeholder = QStringLiteral("glpat-…");
        FieldForm form({token}, context());
        auto *line = qobject_cast<QLineEdit *>(form.editorFor(QStringLiteral("token")));
        auto *remove = form.findChild<QPushButton *>(QStringLiteral("removeSecret_token"));
        QVERIFY(line);
        QVERIFY(remove);
        QCOMPARE(line->echoMode(), QLineEdit::Password);

        // Unknown: the field's own placeholder, and no Remove.
        QCOMPARE(line->placeholderText(), QStringLiteral("glpat-…"));
        QVERIFY(remove->isHidden());

        form.setSecretState(QStringLiteral("token"), FieldForm::SecretState::NotSet);
        QCOMPARE(line->placeholderText(), QStringLiteral("Not set on this device."));
        QVERIFY(remove->isHidden());

        form.setSecretState(QStringLiteral("token"), FieldForm::SecretState::Set);
        QCOMPARE(line->placeholderText(), QStringLiteral("Set on this device. Type to replace it."));
        QVERIFY(!remove->isHidden());
        QVERIFY(line->text().isEmpty());

        // Typing nothing keeps the token: no secret, nothing removed.
        QCOMPARE(form.secrets(), QJsonObject());
        QCOMPARE(form.removedSecrets(), QStringList());
        QVERIFY(!form.config().contains(QStringLiteral("token")));

        // An unknown key changes nothing.
        form.setSecretState(QStringLiteral("other"), FieldForm::SecretState::NotSet);
        QCOMPARE(line->placeholderText(), QStringLiteral("Set on this device. Type to replace it."));
    }

    // Remove marks a set token for removal on OK. Typing a token after it
    // replaces the token instead, and setConfig() starts over.
    void removeSecret()
    {
        FieldForm form({field(QStringLiteral("token"), QStringLiteral("Token"), Field::Type::Secret)}, context());
        auto *line = qobject_cast<QLineEdit *>(form.editorFor(QStringLiteral("token")));
        auto *remove = form.findChild<QPushButton *>(QStringLiteral("removeSecret_token"));
        QVERIFY(line);
        QVERIFY(remove);
        form.setSecretState(QStringLiteral("token"), FieldForm::SecretState::Set);

        QSignalSpy changed(&form, &FieldForm::changed);
        remove->click();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(form.removedSecrets(), QStringList{QStringLiteral("token")});
        QVERIFY(!remove->isEnabled());
        QCOMPARE(line->placeholderText(), QStringLiteral("Removed when you press OK. Type to set a new one."));
        QCOMPARE(form.secrets(), QJsonObject());

        line->setText(QStringLiteral("new"));
        QCOMPARE(form.removedSecrets(), QStringList());
        QCOMPARE(form.secrets(), (QJsonObject{{QStringLiteral("token"), QStringLiteral("new")}}));

        form.setConfig({});
        QCOMPARE(form.removedSecrets(), QStringList());
        QVERIFY(remove->isEnabled());
        QCOMPARE(line->placeholderText(), QStringLiteral("Set on this device. Type to replace it."));
    }

    // One field has a module's own editor, and the rest stay automatic.
    void customFieldEditor()
    {
        Field required = field(QStringLiteral("name"), QStringLiteral("Name"), Field::Type::Text, true);
        FieldForm form({required, field(QStringLiteral("other"), QStringLiteral("Other"), Field::Type::Text)},
                       context());
        form.setConfig({{QStringLiteral("name"), QStringLiteral("ANN")}, {QStringLiteral("keep"), 1}});
        auto *shout = new ShoutEditor;
        form.setFieldEditor(QStringLiteral("name"), shout);

        QCOMPARE(form.editorFor(QStringLiteral("name")), shout);
        QVERIFY(qobject_cast<QLineEdit *>(form.editorFor(QStringLiteral("other"))));
        QCOMPARE(shout->line->text(), QStringLiteral("ann"));
        // An untouched value is kept as stored, even one the editor changes.
        QCOMPARE(form.config().value(QStringLiteral("name")).toString(), QStringLiteral("ANN"));

        QSignalSpy changed(&form, &ItemEditor::changed);
        QTest::keyClicks(shout->line, QStringLiteral("a"));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(form.config().value(QStringLiteral("name")).toString(), QStringLiteral("ANNA"));
        QCOMPARE(form.config().value(QStringLiteral("keep")).toInt(), 1);

        // The field's own rules still apply, and so do the editor's.
        QVERIFY(form.problems().isEmpty());
        shout->line->setText(QStringLiteral("bad"));
        QCOMPARE(form.problems(), QStringList{QStringLiteral("Bad.")});
        shout->line->clear();
        emit shout->changed();
        QCOMPARE(form.problems().size(), 1);
        QVERIFY(form.problems().constFirst().contains(QStringLiteral("Name")));

        // setConfig() reaches it, without counting as an edit.
        changed.clear();
        form.setConfig({{QStringLiteral("name"), QStringLiteral("BOB")}});
        QCOMPARE(shout->line->text(), QStringLiteral("bob"));
        QCOMPARE(changed.count(), 0);
    }

    void customFieldEditorRefused()
    {
        FieldForm form({field(QStringLiteral("token"), QStringLiteral("Token"), Field::Type::Secret)}, context());
        QWidget *before = form.editorFor(QStringLiteral("token"));

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("is a Secret field")));
        form.setFieldEditor(QStringLiteral("token"), new ShoutEditor);
        QCOMPARE(form.editorFor(QStringLiteral("token")), before);

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("no field")));
        form.setFieldEditor(QStringLiteral("nothing"), new ShoutEditor);
        QVERIFY(!form.editorFor(QStringLiteral("nothing")));
    }

    // An image field's box goes too, and a group keeps its place.
    void customFieldEditorInAGroup()
    {
        Field image = field(QStringLiteral("file"), QStringLiteral("Image"), Field::Type::Image);
        image.group = QStringLiteral("Look");
        FieldForm form({image}, context());
        auto *shout = new ShoutEditor;
        form.setFieldEditor(QStringLiteral("file"), shout);
        auto *box = form.findChild<QGroupBox *>(QStringLiteral("group.Look"));
        QVERIFY(box);
        QVERIFY(box->layout()->indexOf(shout) >= 0);
        QVERIFY(!form.findChild<QPushButton *>(QStringLiteral("pasteImage")));
    }

    void helpAndGroups()
    {
        Field plain = field(QStringLiteral("plain"), QStringLiteral("Plain"), Field::Type::Text);
        plain.help = QStringLiteral("Plain help");
        Field first = field(QStringLiteral("first"), QStringLiteral("First"), Field::Type::Text);
        first.group = QStringLiteral("One");
        Field second = field(QStringLiteral("second"), QStringLiteral("Second"), Field::Type::Text);
        second.group = QStringLiteral("Two");
        Field third = field(QStringLiteral("third"), QStringLiteral("Third"), Field::Type::Text);
        third.group = QStringLiteral("One");
        Field last = field(QStringLiteral("last"), QStringLiteral("Last"), Field::Type::Text);
        FieldForm form({first, plain, second, third, last}, context());

        QCOMPARE(form.findChild<QLabel *>(QStringLiteral("help.plain"))->text(), QStringLiteral("Plain help"));
        QVERIFY(!form.findChild<QLabel *>(QStringLiteral("help.first")));

        auto *one = form.findChild<QGroupBox *>(QStringLiteral("group.One"));
        auto *two = form.findChild<QGroupBox *>(QStringLiteral("group.Two"));
        QVERIFY(one && two);
        QCOMPARE(one->title(), QStringLiteral("One"));
        QVERIFY(one->layout()->indexOf(form.editorFor(QStringLiteral("first"))) >= 0);
        QVERIFY(one->layout()->indexOf(form.editorFor(QStringLiteral("third"))) >= 0);
        QVERIFY(two->layout()->indexOf(form.editorFor(QStringLiteral("second"))) >= 0);

        // Fields without a group come first, then the groups in order.
        auto *layout = qobject_cast<QFormLayout *>(form.layout());
        QVERIFY(layout->indexOf(form.editorFor(QStringLiteral("last"))) < layout->indexOf(one));
        QVERIFY(layout->indexOf(one) < layout->indexOf(two));

        // A group changes no keys.
        qobject_cast<QLineEdit *>(form.editorFor(QStringLiteral("third")))->setText(QStringLiteral("x"));
        QCOMPARE(form.config(), (QJsonObject{{QStringLiteral("third"), QStringLiteral("x")}}));
    }
};

QTEST_MAIN(TestFieldForm)
#include "tst_field_form.moc"
