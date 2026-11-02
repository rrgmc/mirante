// A module's own editors of its project and device settings, and what any
// editor is told about its profile. See decision 0122.
#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "core/Profiles.h"
#include "items/ItemTypeRegistry.h"
#include "ui/ItemDialog.h"
#include "ui/ProjectDialog.h"
#include "ui/SettingsDialog.h"

#include <mirante/ItemEditor.h>
#include <mirante/widgets/FieldForm.h>

#include <QComboBox>
#include <QGroupBox>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using namespace mirante;
using mirante::widgets::FieldForm;

namespace {

// One line for the "label" key. It reports a problem while the line says "bad",
// and remembers the profile it is told about.
class LabelEditor : public ItemEditor
{
    Q_OBJECT

public:
    LabelEditor(const EditorContext &context, QWidget *parent)
        : ItemEditor(parent)
        , line(new QLineEdit(this))
        , context(context)
        , profile(context.profileId)
    {
        line->setObjectName(QStringLiteral("own"));
        connect(line, &QLineEdit::textEdited, this, &ItemEditor::changed);
    }

    void setConfig(const QJsonObject &config) override
    {
        m_config = config;
        line->setText(config.value(QStringLiteral("label")).toString());
    }
    QJsonObject config() const override
    {
        QJsonObject config = m_config;
        config.insert(QStringLiteral("label"), line->text());
        return config;
    }
    QStringList problems() const override
    {
        return line->text() == QLatin1String("bad") ? QStringList{QStringLiteral("A bad label.")} : QStringList();
    }
    void setProfile(const QString &profileId) override { profile = profileId; }

    QLineEdit *line;
    EditorContext context;
    QString profile;

private:
    QJsonObject m_config;
};

Field field(const QString &key, Field::Type type = Field::Type::Text)
{
    Field f;
    f.key = key;
    f.label = key;
    f.type = type;
    return f;
}

ItemEditor *makeLabelEditor(const EditorContext &context, QWidget *parent)
{
    return new LabelEditor(context, parent);
}

class OwnEditorType : public ItemType
{
public:
    QString id() const override { return QStringLiteral("own.thing"); }
    QString name() const override { return QStringLiteral("Own thing"); }
    QIcon icon() const override { return {}; }
    ItemEditor *createEditor(const EditorContext &context, QWidget *parent) const override
    {
        return new LabelEditor(context, parent);
    }
    Rendering rendering() const override { return Rendering::Web; }
};

Profile profile(const QString &id, const QString &name)
{
    Profile p;
    p.id = id;
    p.name = name;
    return p;
}

} // namespace

class TestCustomEditors : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_ini = std::make_unique<QSettings>(m_dir->filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        m_model = std::make_unique<Model>();
        m_model->setProfiles({profile(QStringLiteral("work"), QStringLiteral("Work")),
                              profile(QStringLiteral("home"), QStringLiteral("Home"))});
        Group work;
        work.id = QStringLiteral("g1");
        work.name = QStringLiteral("Work");
        work.profile = QStringLiteral("work");
        Group home;
        home.id = QStringLiteral("g2");
        home.name = QStringLiteral("Home");
        home.profile = QStringLiteral("home");
        m_model->setGroup(work);
        m_model->setGroup(home);
    }

    void cleanup()
    {
        m_model.reset();
        m_ini.reset();
        m_dir.reset();
    }

    void projectEditorIsShownAndStored()
    {
        ItemTypeRegistry types;
        types.registerModule(QStringLiteral("own"), [](ModuleRegistrar &registrar) {
            registrar.setModuleName(QStringLiteral("Own"));
            registrar.setProjectEditor(&makeLabelEditor);
        });
        // Without one field: the editor alone gives the module a section.
        QCOMPARE(types.projectSettingModules(), QStringList{QStringLiteral("own")});

        Project project;
        project.id = QStringLiteral("p1");
        project.name = QStringLiteral("Project");
        project.groupId = QStringLiteral("g1");
        project.moduleSettings = QJsonObject{
            {QStringLiteral("own"), QJsonObject{{QStringLiteral("label"), QStringLiteral("old")},
                                                {QStringLiteral("kept"), 1}}}};
        EditorContext context;
        context.folders.emplace_back(QStringLiteral("f1"), QStringLiteral("Code"));
        const DeviceSettings device(m_ini.get());
        ProjectDialog dialog(project, *m_model, device, types, context);

        auto *editor = dialog.findChild<LabelEditor *>(QStringLiteral("moduleSettings.own"));
        QVERIFY(editor);
        QVERIFY(!dialog.findChild<FieldForm *>(QStringLiteral("moduleSettings.own")));
        QCOMPARE(editor->line->text(), QStringLiteral("old"));
        QCOMPARE(editor->context.folders.size(), std::size_t(1));
        // The core keeps the frame.
        QCOMPARE(dialog.findChild<QGroupBox *>(QStringLiteral("section.own"))->title(), QStringLiteral("Own"));

        // It knows the group's profile, and hears when the group changes.
        QCOMPARE(editor->profile, QStringLiteral("work"));
        auto *group = dialog.findChild<QComboBox *>(QStringLiteral("group"));
        group->setCurrentIndex(group->findData(QStringLiteral("g2")));
        QCOMPARE(editor->profile, QStringLiteral("home"));

        // Its config() is stored, and its problems refuse OK.
        editor->line->setText(QStringLiteral("new"));
        const QJsonObject expected{{QStringLiteral("label"), QStringLiteral("new")}, {QStringLiteral("kept"), 1}};
        QCOMPARE(dialog.value().moduleSettings.value(QStringLiteral("own")).toObject(), expected);
        editor->line->setText(QStringLiteral("bad"));
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("message"))->text().contains(QStringLiteral("A bad label.")));
    }

    // A section not shown leaves the stored settings alone, whatever the editor says.
    void projectEditorNotShownStoresNothing()
    {
        ItemTypeRegistry types;
        types.registerModule(QStringLiteral("own"), [](ModuleRegistrar &registrar) {
            registrar.setProjectEditor(&makeLabelEditor);
        });
        Project project;
        project.id = QStringLiteral("p1");
        project.name = QStringLiteral("Project");
        project.groupId = QStringLiteral("g1");
        const DeviceSettings device(m_ini.get());
        ProjectDialog dialog(project, *m_model, device, types);
        QVERIFY(dialog.value().moduleSettings.isEmpty());
    }

    // The editor saves on changed(), and only declared keys are stored.
    void deviceEditorSavesOnChanged()
    {
        ItemTypeRegistry types;
        types.registerModule(QStringLiteral("own"), [](ModuleRegistrar &registrar) {
            registrar.addDeviceSetting(field(QStringLiteral("label")));
            registrar.setDeviceEditor(&makeLabelEditor);
        });
        DeviceSettings settings(m_ini.get());
        settings.setModuleSetting(QStringLiteral("own"), QStringLiteral("label"), QStringLiteral("old"));

        SettingsDialog dialog(settings);
        dialog.setModules(types);
        auto *editor = dialog.findChild<LabelEditor *>(QStringLiteral("deviceSettingsForm.own"));
        QVERIFY(editor);
        QCOMPARE(editor->line->text(), QStringLiteral("old"));
        QVERIFY(editor->context.profileId.isEmpty());

        QTest::keyClicks(editor->line, QStringLiteral("er"));
        QCOMPARE(settings.moduleSetting(QStringLiteral("own"), QStringLiteral("label")).toString(),
                 QStringLiteral("older"));
    }

    // A device setting can be any type but the three that need a profile or a
    // project, and survives the store with its JSON type.
    void deviceSettingTypes()
    {
        ItemTypeRegistry types;
        types.registerModule(QStringLiteral("own"), [](ModuleRegistrar &registrar) {
            registrar.addDeviceSetting(field(QStringLiteral("list"), Field::Type::TextList));
            registrar.addDeviceSetting(field(QStringLiteral("site"), Field::Type::Url));
        });
        QCOMPARE(types.deviceSettings(QStringLiteral("own")).size(), std::size_t(2));

        DeviceSettings settings(m_ini.get());
        const QJsonArray list{QStringLiteral("a"), QStringLiteral("b")};
        settings.setModuleSetting(QStringLiteral("own"), QStringLiteral("list"), list);
        QCOMPARE(settings.moduleSetting(QStringLiteral("own"), QStringLiteral("list")).toArray(), list);

        ItemTypeRegistry refusing;
        const QRegularExpression leftOut(QStringLiteral("device setting .* left out"));
        QTest::ignoreMessage(QtWarningMsg, leftOut);
        QTest::ignoreMessage(QtWarningMsg, leftOut);
        QTest::ignoreMessage(QtWarningMsg, leftOut);
        refusing.registerModule(QStringLiteral("own"), [](ModuleRegistrar &registrar) {
            registrar.addDeviceSetting(field(QStringLiteral("a"), Field::Type::Secret));
            registrar.addDeviceSetting(field(QStringLiteral("b"), Field::Type::Folder));
            registrar.addDeviceSetting(field(QStringLiteral("c"), Field::Type::Image));
        });
        QVERIFY(refusing.deviceSettings(QStringLiteral("own")).empty());
    }

    // A type's own editor stays when the profile changes, and is told. It also
    // knows the profile from the start.
    void itemEditorIsToldTheProfile()
    {
        ItemTypeRegistry types;
        types.registerModule(QStringLiteral("own"), [](ModuleRegistrar &registrar) {
            registrar.addItemType(std::make_unique<OwnEditorType>());
        });
        Item item;
        item.type = QStringLiteral("own.thing");
        ItemDialog dialog(item, false, types, {});
        dialog.setProfiles(profiles::all(*m_model), QStringLiteral("work"), QStringLiteral("Work"));

        auto *editor = dialog.findChild<LabelEditor *>(QStringLiteral("editor"));
        QVERIFY(editor);
        dialog.setProfile(QStringLiteral("home"));
        QCOMPARE(dialog.findChild<LabelEditor *>(QStringLiteral("editor")), editor);
        QCOMPARE(editor->profile, QStringLiteral("home"));
        dialog.setProfile(QString());
        QCOMPARE(editor->profile, QStringLiteral("work"));
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<QSettings> m_ini;
    std::unique_ptr<Model> m_model;
};

QTEST_MAIN(TestCustomEditors)
#include "tst_custom_editors.moc"
