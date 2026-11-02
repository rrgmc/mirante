#pragma once

#include <mirante/ItemEditor.h>
#include <mirante/Module.h>
#include <mirante/ModuleIds.h>
#include <mirante/QuickAdd.h>

#include <QJsonObject>
#include <QVersionNumber>
#include <QtGlobal>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace mirante::testing {

// Registers one module without the core, and gives back what it registered, for
// a module's tests. It stops the program on a broken id, as the core does.
//
//     mirante::testing::TestRegistrar registry;
//     registry.registerModule(QStringLiteral("gitlab"), &mirante::modules::gitlab::registerModule);
//     const mirante::ItemType &type = registry.type(QStringLiteral("gitlab.mr.summary"));
class TestRegistrar : public ModuleRegistrar
{
public:
    using RegisterFunction = void (*)(ModuleRegistrar &);
    using SettingsReader =
        std::function<QJsonObject(const QString &profileId, const QString &moduleId, const QString &screenId)>;

    void registerModule(const QString &moduleId, RegisterFunction fn)
    {
        if (!isValidModuleId(moduleId))
            qFatal("Invalid module id \"%s\": use lowercase letters and digits only", qUtf8Printable(moduleId));
        if (!m_moduleId.isEmpty())
            qFatal("A TestRegistrar takes one module, so %s is one too many", qUtf8Printable(moduleId));
        m_moduleId = moduleId;
        fn(*this);
    }

    // Reads the stored values of one screen in a profile, for settings().
    void setSettingsReader(SettingsReader reader) { m_settings.reader = std::move(reader); }

    // ModuleRegistrar.

    void addItemType(std::unique_ptr<ItemType> type) override
    {
        if (!type)
            qFatal("Module %s registered a null item type", qUtf8Printable(m_moduleId));
        const QString id = type->id();
        if (!idBelongsToModule(id, m_moduleId))
            qFatal("Module %s registered the item type \"%s\", which does not start with \"%s.\"",
                   qUtf8Printable(m_moduleId), qUtf8Printable(id), qUtf8Printable(m_moduleId));
        if (contains(id))
            qFatal("Module %s registered the item type \"%s\" twice", qUtf8Printable(m_moduleId), qUtf8Printable(id));
        m_types.push_back(std::move(type));
    }
    void setModuleName(const QString &name) override { m_name = name; }
    void setModuleDescription(const QString &description) override { m_description = description; }
    void setModuleUuid(const QUuid &uuid) override
    {
        if (uuid.isNull())
            qFatal("Module %s set a null UUID", qUtf8Printable(m_moduleId));
        m_uuid = uuid;
    }
    void setModuleVersion(const QString &version) override
    {
        qsizetype suffix = 0;
        if (QVersionNumber::fromString(version, &suffix).isNull() || suffix != version.size())
            qFatal("Module %s set the version \"%s\": use numbers separated by dots, such as 1.2.0",
                   qUtf8Printable(m_moduleId), qUtf8Printable(version));
        m_version = version;
    }
    void setModuleAuthor(const QString &author) override { m_author = author; }
    void setModuleHomepage(const QUrl &homepage) override
    {
        const QString scheme = homepage.scheme();
        if (!homepage.isValid() || homepage.host().isEmpty()
            || (scheme != QLatin1String("http") && scheme != QLatin1String("https")))
            qFatal("Module %s set the homepage \"%s\": use an http or https address", qUtf8Printable(m_moduleId),
                   qUtf8Printable(homepage.toString()));
        m_homepage = homepage;
    }
    void addSettingsScreen(std::unique_ptr<SettingsScreen> screen) override
    {
        if (!screen)
            qFatal("Module %s added a null settings screen", qUtf8Printable(m_moduleId));
        const QString id = screen->id();
        if (!isValidModuleId(id))
            qFatal("Module %s added the settings screen \"%s\": use lowercase letters and digits only",
                   qUtf8Printable(m_moduleId), qUtf8Printable(id));
        if (screen->name().isEmpty())
            qFatal("Module %s added the settings screen \"%s\" without a name", qUtf8Printable(m_moduleId),
                   qUtf8Printable(id));
        if (settingsScreen(m_moduleId, id))
            qFatal("Module %s added the settings screen \"%s\" twice", qUtf8Printable(m_moduleId), qUtf8Printable(id));
        m_screens.push_back(std::move(screen));
    }
    const ModuleSettings &settings() const override
    {
        m_settings.moduleId = m_moduleId;
        return m_settings;
    }
    void setServerType(std::unique_ptr<ServerType> type) override
    {
        if (!type)
            qFatal("Module %s set a null server type", qUtf8Printable(m_moduleId));
        if (m_server)
            qFatal("Module %s set its server type twice", qUtf8Printable(m_moduleId));
        m_server = std::move(type);
    }
    void addProjectSetting(const Field &field) override
    {
        if (projectSettingTakes(field.type))
            m_projectSettings.push_back(field);
        else
            qFatal("Module %s: the project setting \"%s\" has a type that project settings do not take",
                   qUtf8Printable(m_moduleId), qUtf8Printable(field.key));
    }
    void setProjectEditor(EditorFactory factory) override { m_projectEditor = std::move(factory); }
    void addQuickAddHandler(std::unique_ptr<QuickAddHandler> handler) override
    {
        m_handlers.push_back(std::move(handler));
    }
    void addRelatedItemFinder(std::unique_ptr<RelatedItemFinder> finder) override
    {
        m_finders.push_back(std::move(finder));
    }
    void addDeviceSetting(const Field &field) override
    {
        if (deviceSettingTakes(field.type))
            m_deviceSettings.push_back(field);
        else
            qFatal("Module %s: the device setting \"%s\" has a type that device settings do not take",
                   qUtf8Printable(m_moduleId), qUtf8Printable(field.key));
    }
    void setDeviceEditor(EditorFactory factory) override { m_deviceEditor = std::move(factory); }

    // What the module registered.

    // The types, sorted by name.
    std::vector<const ItemType *> types() const
    {
        std::vector<const ItemType *> all;
        for (const std::unique_ptr<ItemType> &type : m_types)
            all.push_back(type.get());
        std::sort(all.begin(), all.end(), [](const ItemType *a, const ItemType *b) {
            return a->name() != b->name() ? a->name() < b->name() : a->id() < b->id();
        });
        return all;
    }
    const ItemType *findType(const QString &id) const
    {
        for (const std::unique_ptr<ItemType> &type : m_types) {
            if (type->id() == id)
                return type.get();
        }
        return nullptr;
    }
    bool contains(const QString &id) const { return findType(id) != nullptr; }
    // A registered type. A test that asks for any other id has a bug.
    const ItemType &type(const QString &id) const
    {
        const ItemType *found = findType(id);
        if (!found)
            qFatal("Module %s did not register the item type \"%s\"", qUtf8Printable(m_moduleId), qUtf8Printable(id));
        return *found;
    }

    QString moduleName(const QString &) const { return m_name.isEmpty() ? m_moduleId : m_name; }
    QString moduleDescription(const QString &) const { return m_description; }
    QUuid moduleUuid(const QString &) const { return m_uuid; }
    QString moduleVersion(const QString &) const { return m_version; }
    QString moduleAuthor(const QString &) const { return m_author; }
    QUrl moduleHomepage(const QString &) const { return m_homepage; }

    std::vector<const SettingsScreen *> settingsScreens(const QString &) const
    {
        std::vector<const SettingsScreen *> all;
        for (const std::unique_ptr<SettingsScreen> &screen : m_screens)
            all.push_back(screen.get());
        return all;
    }
    const SettingsScreen *settingsScreen(const QString &, const QString &screenId) const
    {
        for (const std::unique_ptr<SettingsScreen> &screen : m_screens) {
            if (screen->id() == screenId)
                return screen.get();
        }
        return nullptr;
    }
    const ServerType *serverType(const QString &) const { return m_server.get(); }
    std::vector<Field> projectSettings(const QString &) const { return m_projectSettings; }
    std::vector<Field> deviceSettings(const QString &) const { return m_deviceSettings; }
    EditorFactory projectEditor(const QString &) const { return m_projectEditor; }
    EditorFactory deviceEditor(const QString &) const { return m_deviceEditor; }
    const std::vector<std::unique_ptr<QuickAddHandler>> &quickAddHandlers() const { return m_handlers; }
    const std::vector<std::unique_ptr<RelatedItemFinder>> &relatedItemFinders() const { return m_finders; }

private:
    class Settings : public ModuleSettings
    {
    public:
        QJsonObject values(const QString &profileId, const QString &screenId) const override
        {
            return reader ? reader(profileId, moduleId, screenId) : QJsonObject();
        }
        QString moduleId;
        SettingsReader reader;
    };

    QString m_moduleId;
    QString m_name;
    QString m_description;
    QUuid m_uuid;
    QString m_version;
    QString m_author;
    QUrl m_homepage;
    std::vector<std::unique_ptr<ItemType>> m_types;
    std::vector<std::unique_ptr<SettingsScreen>> m_screens;
    mutable Settings m_settings;
    std::unique_ptr<ServerType> m_server;
    std::vector<Field> m_projectSettings;
    std::vector<Field> m_deviceSettings;
    EditorFactory m_projectEditor;
    EditorFactory m_deviceEditor;
    std::vector<std::unique_ptr<QuickAddHandler>> m_handlers;
    std::vector<std::unique_ptr<RelatedItemFinder>> m_finders;
};

} // namespace mirante::testing
