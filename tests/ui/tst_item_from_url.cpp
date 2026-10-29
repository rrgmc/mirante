// Tests making an item from a pasted link: the link field of the item
// dialog's type page, which shows only the types that take the link, best
// first, and fills the chosen type's configuration and name; and "Paste
// URL…" in a task's menus, which opens the dialog with the clipboard's link
// and gives the item the connection of the link's server.
#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"
#include "ui/EntityActions.h"
#include "ui/FieldForm.h"
#include "ui/ItemDialog.h"

#include <mirante/ItemEditor.h>
#include <mirante/Module.h>

#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace mirante;

namespace {

const QString fixLoginTimeout = QStringLiteral("760c8e20-8555-5a2c-bcbe-75e7fd08ca74");
// The gitlab connections of the Work profile, which the Work group uses:
// gitlab.com is the default.
const QString gitlabCom = QStringLiteral("8ef145f6-9585-55de-a438-70969ec1bad0");
const QString gitlabAcme = QStringLiteral("6a2269ce-1dd5-5ebf-b064-2ce63914bf73");

const QString requestType = QStringLiteral("testmod.request");
const QString pageType = QStringLiteral("testmod.page");
const QString plainType = QStringLiteral("testmod.plain");

// A merge request of a "gitlab" connection: <base>/<project>/-/merge_requests/<n>
// below one of its base URLs, or on gitlab.example.
class RequestType : public ItemType
{
public:
    QString id() const override { return requestType; }
    QString name() const override { return QStringLiteral("Request"); }
    QIcon icon() const override { return {}; }
    QStringList connectionKinds() const override { return {QStringLiteral("gitlab")}; }

    std::vector<Field> fields() const override
    {
        Field project;
        project.key = QStringLiteral("project");
        project.label = QStringLiteral("Project");
        Field iid;
        iid.key = QStringLiteral("iid");
        iid.label = QStringLiteral("Number");
        iid.type = Field::Type::Integer;
        Field note;
        note.key = QStringLiteral("note");
        note.label = QStringLiteral("Note");
        note.defaultValue = QStringLiteral("default note");
        return {project, iid, note};
    }

    std::optional<UrlMatch> fromUrl(const QUrl &url, const QList<QUrl> &baseUrls) const override
    {
        QList<QUrl> bases = baseUrls;
        bases << QUrl(QStringLiteral("https://gitlab.example"));
        for (const QUrl &base : bases) {
            const std::optional<QString> path = pathBelow(url, base);
            if (!path)
                continue;
            const qsizetype at = path->indexOf(QStringLiteral("/-/merge_requests/"));
            if (at <= 0)
                return std::nullopt;
            UrlMatch match;
            match.config.insert(QStringLiteral("project"), path->left(at));
            match.config.insert(QStringLiteral("iid"), path->mid(at + 18).toInt());
            match.name = path->left(at) + QLatin1Char('!') + path->mid(at + 18);
            match.baseUrl = base;
            return match;
        }
        return std::nullopt;
    }
};

// Any https page, as a generic match. Its name sorts first.
class PageType : public ItemType
{
public:
    QString id() const override { return pageType; }
    QString name() const override { return QStringLiteral("A page"); }
    QIcon icon() const override { return {}; }

    std::vector<Field> fields() const override
    {
        Field url;
        url.key = QStringLiteral("url");
        url.label = QStringLiteral("Address");
        url.type = Field::Type::Url;
        return {url};
    }

    std::optional<UrlMatch> fromUrl(const QUrl &url, const QList<QUrl> &) const override
    {
        if (url.scheme() != QLatin1String("https"))
            return std::nullopt;
        UrlMatch match;
        match.config.insert(QStringLiteral("url"), url.toString());
        match.name = url.host();
        match.generic = true;
        return match;
    }
};

// Takes no link.
class PlainType : public ItemType
{
public:
    QString id() const override { return plainType; }
    QString name() const override { return QStringLiteral("Plain"); }
    QIcon icon() const override { return {}; }
};

void registerTestModule(ModuleRegistrar &registrar)
{
    registrar.addItemType(std::make_unique<RequestType>());
    registrar.addItemType(std::make_unique<PageType>());
    registrar.addItemType(std::make_unique<PlainType>());
}

QStringList shownTypes(ItemDialog &dialog)
{
    QStringList ids;
    const auto *list = dialog.findChild<QListWidget *>(QStringLiteral("types"));
    for (int i = 0; i < list->count(); ++i)
        ids << list->item(i)->data(Qt::UserRole).toString();
    return ids;
}

QString hint(ItemDialog &dialog) { return dialog.findChild<QLabel *>(QStringLiteral("typeHint"))->text(); }

QString nameOf(ItemDialog &dialog) { return dialog.findChild<QLineEdit *>(QStringLiteral("name"))->text(); }

QAction *actionNamed(const QMenu &menu, const QString &text)
{
    for (QAction *action : menu.actions()) {
        if (action->text() == text)
            return action;
    }
    return nullptr;
}

} // namespace

class TestItemFromUrl : public QObject
{
    Q_OBJECT

    Model model;
    ItemTypeRegistry types;
    std::unique_ptr<Store> store;
    std::unique_ptr<QTemporaryDir> dir;
    std::unique_ptr<QSettings> qsettings;
    std::unique_ptr<DeviceSettings> settings;
    std::unique_ptr<EntityActions> actions;

    // What the item dialog run by the actions showed, and how it is answered.
    QString dialogUrl;
    QString chosenType;
    int execCount = 0;

private slots:
    void initTestCase();
    void init();

    void filtering();
    void prefill();
    void typedNameKept();
    void connectionOfTheLink();
    void pasteUrl();
    void linkInAnotherProfile();
    void menuEntry();
};

void TestItemFromUrl::initTestCase()
{
    types.registerModule(QStringLiteral("testmod"), registerTestModule);
    store = std::make_unique<Store>(model);
}

void TestItemFromUrl::init()
{
    QVERIFY(store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), model).empty());
    dir = std::make_unique<QTemporaryDir>();
    QVERIFY(dir->isValid());
    qsettings = std::make_unique<QSettings>(dir->filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    settings = std::make_unique<DeviceSettings>(qsettings.get());
    actions = std::make_unique<EntityActions>(model, *store, types, *settings, nullptr);
    dialogUrl.clear();
    chosenType = requestType;
    execCount = 0;
    actions->setExec([this](QDialog &dialog) {
        ++execCount;
        auto *item = qobject_cast<ItemDialog *>(&dialog);
        if (!item)
            return false;
        dialogUrl = item->findChild<QLineEdit *>(QStringLiteral("url"))->text();
        item->setType(chosenType);
        item->accept();
        return item->result() == QDialog::Accepted;
    });
    QGuiApplication::clipboard()->clear();
}

void TestItemFromUrl::filtering()
{
    ItemDialog dialog(Item{}, true, types, {});
    dialog.show();
    QCOMPARE(shownTypes(dialog), (QStringList{pageType, plainType, requestType}));
    QCOMPARE(hint(dialog), QStringLiteral("Double-click a type, or select it and choose Next."));

    // A link shows the types that take it, the module's before the generic one,
    // and selects the best.
    auto *url = dialog.findChild<QLineEdit *>(QStringLiteral("url"));
    QVERIFY(url);
    url->setText(QStringLiteral("https://gitlab.example/acme/app/-/merge_requests/4"));
    QCOMPARE(shownTypes(dialog), (QStringList{requestType, pageType}));
    QCOMPARE(dialog.findChild<QListWidget *>(QStringLiteral("types"))->currentRow(), 0);
    QVERIFY(hint(dialog).startsWith(QStringLiteral("These types can show the link")));

    // A link no type takes shows none, and Next is disabled.
    url->setText(QStringLiteral("http://intranet/page"));
    QVERIFY(shownTypes(dialog).isEmpty());
    QCOMPARE(hint(dialog), QStringLiteral("No item type can show this link."));
    QVERIFY(!dialog.findChild<QPushButton *>(QStringLiteral("next"))->isEnabled());

    // Text that is not a web address shows every type.
    url->setText(QStringLiteral("not a link"));
    QCOMPARE(shownTypes(dialog).size(), 3);
    QCOMPARE(hint(dialog), QStringLiteral("This is not a web address, so every type is shown."));

    // Clearing the field shows every type again.
    url->clear();
    QCOMPARE(shownTypes(dialog), (QStringList{pageType, plainType, requestType}));
    QVERIFY(dialog.findChild<QPushButton *>(QStringLiteral("next"))->isEnabled());
}

void TestItemFromUrl::prefill()
{
    ItemDialog dialog(Item{}, true, types, {});
    dialog.show();
    dialog.setUrl(QStringLiteral("  https://gitlab.example/acme/app/-/merge_requests/4 "));

    // Next goes on with the best match, filled from the link over its defaults.
    dialog.accept();
    QVERIFY(!dialog.choosingType());
    QCOMPARE(dialog.value().type, requestType);
    QCOMPARE(dialog.value().config, (QJsonObject{{QStringLiteral("project"), QStringLiteral("acme/app")},
                                                 {QStringLiteral("iid"), 4},
                                                 {QStringLiteral("note"), QStringLiteral("default note")}}));
    QCOMPARE(nameOf(dialog), QStringLiteral("acme/app!4"));
    QVERIFY(dialog.urlConnectionId().isEmpty());     // no connections given

    // The same type from the same link keeps what was entered.
    auto *form = qobject_cast<FieldForm *>(dialog.editor());
    QVERIFY(form);
    auto *note = qobject_cast<QLineEdit *>(form->editorFor(QStringLiteral("note")));
    QVERIFY(note);
    note->setText(QStringLiteral("mine"));
    dialog.back();
    dialog.accept();
    QCOMPARE(dialog.value().config.value(QStringLiteral("note")).toString(), QStringLiteral("mine"));

    // Another type takes its own name and configuration from the link.
    dialog.back();
    dialog.setType(pageType);
    QCOMPARE(dialog.value().config,
             (QJsonObject{{QStringLiteral("url"), QStringLiteral("https://gitlab.example/acme/app/-/merge_requests/4")}}));
    QCOMPARE(nameOf(dialog), QStringLiteral("gitlab.example"));

    // A type that does not take the link starts from its defaults.
    dialog.back();
    dialog.setType(plainType);
    QCOMPARE(dialog.value().config, QJsonObject());

    // After clearing the link, the type that was filled starts from its defaults.
    dialog.back();
    dialog.setUrl(QString());
    dialog.setType(requestType);
    QCOMPARE(dialog.value().config, (QJsonObject{{QStringLiteral("note"), QStringLiteral("default note")}}));
}

void TestItemFromUrl::typedNameKept()
{
    ItemDialog dialog(Item{}, true, types, {});
    dialog.setUrl(QStringLiteral("https://gitlab.example/acme/app/-/merge_requests/4"));
    dialog.setType(pageType);
    QCOMPARE(nameOf(dialog), QStringLiteral("gitlab.example"));
    dialog.setName(QStringLiteral("Typed"));
    dialog.back();
    dialog.setType(requestType);
    QCOMPARE(nameOf(dialog), QStringLiteral("Typed"));
}

void TestItemFromUrl::connectionOfTheLink()
{
    const Group *work = model.group(model.project(model.task(fixLoginTimeout)->projectId)->groupId);
    QVERIFY(work);
    const Profile *profile = model.profile(work->profile);
    QVERIFY(profile);
    ItemDialog dialog(Item{}, true, types, {});
    dialog.setConnections(profile->connections);
    dialog.setUrl(QStringLiteral("https://gitlab.acme.internal/ops/infra/-/merge_requests/2"));
    QCOMPARE(shownTypes(dialog), (QStringList{requestType, pageType}));
    dialog.setType(requestType);
    QCOMPARE(dialog.urlConnectionId(), gitlabAcme);
    QCOMPARE(dialog.value().config.value(QStringLiteral("project")).toString(), QStringLiteral("ops/infra"));

    // A generic type names no connection.
    dialog.back();
    dialog.setType(pageType);
    QVERIFY(dialog.urlConnectionId().isEmpty());
}

// The link is read with the connections of the group's profile. When the
// item gets another profile in the dialog, it takes that profile's
// connection of the same kind and base URL, or none.
void TestItemFromUrl::linkInAnotherProfile()
{
    const Group *group = model.group(model.project(model.task(fixLoginTimeout)->projectId)->groupId);
    const Profile *work = model.profile(group->profile);
    QVERIFY(work);
    Connection acme;
    for (const Connection &connection : work->connections) {
        if (connection.id == gitlabAcme)
            acme = connection;
    }
    Connection com = acme;
    com.id = QStringLiteral("other-com");
    com.baseUrl = QStringLiteral("https://gitlab.com");
    Connection otherAcme = acme;
    otherAcme.id = QStringLiteral("other-acme");
    Profile other;
    other.id = QStringLiteral("other");
    other.name = QStringLiteral("Other");
    other.connections = {com, otherAcme};
    other.defaultConnections.insert(QStringLiteral("gitlab"), com.id);
    model.setProfile(other);

    QString profileId = QStringLiteral("other");
    actions->setExec([&profileId](QDialog &dialog) {
        auto *item = qobject_cast<ItemDialog *>(&dialog);
        if (!item)
            return false;
        item->setType(requestType);
        item->setProfile(profileId);
        item->accept();
        return item->result() == QDialog::Accepted;
    });
    const QString link = QStringLiteral("https://gitlab.acme.internal/ops/infra/-/merge_requests/2");
    QGuiApplication::clipboard()->setText(link);
    const QString first = actions->pasteUrl(fixLoginTimeout);
    QVERIFY(!first.isEmpty());
    QCOMPARE(model.item(fixLoginTimeout, first)->profile, QStringLiteral("other"));
    QCOMPARE(model.item(fixLoginTimeout, first)->connectionId, otherAcme.id);

    // A profile without a connection on that server names none.
    other.connections = {com};
    model.setProfile(other);
    const QString second = actions->pasteUrl(fixLoginTimeout);
    QVERIFY(!second.isEmpty());
    QVERIFY(model.item(fixLoginTimeout, second)->connectionId.isEmpty());
}

void TestItemFromUrl::pasteUrl()
{
    // Without a link on the clipboard, nothing opens.
    QGuiApplication::clipboard()->setText(QStringLiteral("just words"));
    QVERIFY(EntityActions::clipboardUrl().isEmpty());
    QVERIFY(actions->pasteUrl(fixLoginTimeout).isEmpty());
    QCOMPARE(execCount, 0);

    // A link on a server that is not the group default names its connection.
    QGuiApplication::clipboard()->setText(QStringLiteral("https://gitlab.acme.internal/ops/infra/-/merge_requests/2"));
    const QString acme = actions->pasteUrl(fixLoginTimeout);
    QVERIFY(!acme.isEmpty());
    QCOMPARE(dialogUrl, QStringLiteral("https://gitlab.acme.internal/ops/infra/-/merge_requests/2"));
    const Item *item = model.item(fixLoginTimeout, acme);
    QVERIFY(item);
    QCOMPARE(item->type, requestType);
    QCOMPARE(item->name, QStringLiteral("ops/infra!2"));
    QCOMPARE(item->connectionId, gitlabAcme);

    // A link on the default server leaves the item to the resolution order.
    QGuiApplication::clipboard()->setText(QStringLiteral("https://gitlab.com/acme/backend/-/merge_requests/5"));
    const QString com = actions->pasteUrl(fixLoginTimeout);
    QVERIFY(!com.isEmpty());
    QVERIFY(model.item(fixLoginTimeout, com)->connectionId.isEmpty());
    QCOMPARE(model.item(fixLoginTimeout, com)->config.value(QStringLiteral("iid")).toInt(), 5);

    // A copied link, as a browser puts it on the clipboard, works too.
    auto *data = new QMimeData;
    data->setUrls({QUrl(QStringLiteral("https://example.org/doc"))});
    QGuiApplication::clipboard()->setMimeData(data);
    QCOMPARE(EntityActions::clipboardUrl(), QUrl(QStringLiteral("https://example.org/doc")));
}

void TestItemFromUrl::menuEntry()
{
    QGuiApplication::clipboard()->setText(QStringLiteral("no link here"));
    QMenu task;
    actions->fillMenu(&task, SidebarModel::Kind::Task, fixLoginTimeout, fixLoginTimeout);
    QAction *disabled = actionNamed(task, QStringLiteral("Paste URL…"));
    QVERIFY(disabled);
    QVERIFY(!disabled->isEnabled());

    QGuiApplication::clipboard()->setText(QStringLiteral("https://gitlab.com/acme/backend/-/merge_requests/8"));
    QMenu items;
    actions->fillMenu(&items, SidebarModel::Kind::ItemList, QString(), fixLoginTimeout);
    QAction *paste = actionNamed(items, QStringLiteral("Paste URL…"));
    QVERIFY(paste);
    QVERIFY(paste->isEnabled());
    const size_t before = model.task(fixLoginTimeout)->items.size();
    paste->trigger();
    QCOMPARE(execCount, 1);
    QCOMPARE(dialogUrl, QStringLiteral("https://gitlab.com/acme/backend/-/merge_requests/8"));
    QCOMPARE(model.task(fixLoginTimeout)->items.size(), before + 1);
}

QTEST_MAIN(TestItemFromUrl)
#include "tst_item_from_url.moc"
