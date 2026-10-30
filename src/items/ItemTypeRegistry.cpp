#include "items/ItemTypeRegistry.h"

#include "items/ItemTypeIds.h"
#include "items/UnknownItemType.h"

#include <QtGlobal>

#include <algorithm>
#include <map>

namespace mirante {

namespace {

// What ModuleRegistrar::settings() gives a module. It reads through the
// registry's reader, which may be set after the module registered.
class Settings : public ModuleSettings
{
public:
    Settings(const QString &moduleId, const ItemTypeRegistry::SettingsReader *reader)
        : m_moduleId(moduleId)
        , m_reader(reader)
    {
    }

    QJsonObject values(const QString &profileId, const QString &screenId) const override
    {
        return *m_reader ? (*m_reader)(profileId, m_moduleId, screenId) : QJsonObject();
    }

private:
    QString m_moduleId;
    const ItemTypeRegistry::SettingsReader *m_reader;
};

} // namespace

struct ItemTypeRegistry::Private
{
    // One registered module, besides its item types.
    struct Module
    {
        QString name;   // empty when the module gave none
        QString description;
        std::vector<std::unique_ptr<SettingsScreen>> screens;
        std::unique_ptr<Settings> settings;
        std::unique_ptr<ServerType> server;
        std::vector<Field> projectSettings;
    };

    SettingsReader reader;
    std::map<QString, Module> modules;
    std::vector<std::unique_ptr<ItemType>> types;
    std::map<QString, const ItemType *> byId;
    // Made on first use by type(), so callers always get a type back.
    mutable std::map<QString, std::unique_ptr<UnknownItemType>> unknown;

    void add(const QString &moduleId, std::unique_ptr<ItemType> type);
    void addScreen(const QString &moduleId, std::unique_ptr<SettingsScreen> screen);
    void addProjectSetting(const QString &moduleId, const Field &field);
};

void ItemTypeRegistry::Private::add(const QString &moduleId, std::unique_ptr<ItemType> type)
{
    if (!type)
        qFatal("Module %s registered a null item type", qUtf8Printable(moduleId));
    const QString id = type->id();
    if (!idBelongsToModule(id, moduleId))
        qFatal("Module %s registered the item type \"%s\", which does not start with \"%s.\"",
               qUtf8Printable(moduleId), qUtf8Printable(id), qUtf8Printable(moduleId));
    if (byId.count(id) != 0)
        qFatal("Module %s registered the item type \"%s\" twice", qUtf8Printable(moduleId),
               qUtf8Printable(id));
    byId.emplace(id, type.get());
    types.push_back(std::move(type));
}

void ItemTypeRegistry::Private::addScreen(const QString &moduleId, std::unique_ptr<SettingsScreen> screen)
{
    if (!screen)
        qFatal("Module %s added a null settings screen", qUtf8Printable(moduleId));
    const QString id = screen->id();
    // Screen ids follow the rule for module ids: lowercase letters and digits.
    if (!isValidModuleId(id))
        qFatal("Module %s added the settings screen \"%s\": use lowercase letters and digits only",
               qUtf8Printable(moduleId), qUtf8Printable(id));
    if (screen->name().isEmpty())
        qFatal("Module %s added the settings screen \"%s\" without a name", qUtf8Printable(moduleId),
               qUtf8Printable(id));
    Module &module = modules[moduleId];
    for (const auto &existing : module.screens) {
        if (existing->id() == id)
            qFatal("Module %s added the settings screen \"%s\" twice", qUtf8Printable(moduleId),
                   qUtf8Printable(id));
    }
    module.screens.push_back(std::move(screen));
}

void ItemTypeRegistry::Private::addProjectSetting(const QString &moduleId, const Field &field)
{
    // A field the project dialog cannot show is left out with a warning, so
    // the rest of the module still works. Tokens stay in settings screens,
    // and a project has no folders or images of its own to choose from.
    QString problem;
    if (field.key.isEmpty())
        problem = QStringLiteral("has no key");
    else if (field.type == Field::Type::Secret || field.type == Field::Type::Folder
             || field.type == Field::Type::Image)
        problem = QStringLiteral("has a type that project settings do not take");
    Module &module = modules[moduleId];
    if (problem.isEmpty()) {
        for (const Field &existing : module.projectSettings) {
            if (existing.key == field.key)
                problem = QStringLiteral("was added twice");
        }
    }
    if (!problem.isEmpty()) {
        qWarning("Module %s: the project setting \"%s\" %s, so it is left out", qUtf8Printable(moduleId),
                 qUtf8Printable(field.key), qUtf8Printable(problem));
        return;
    }
    module.projectSettings.push_back(field);
}

ItemTypeRegistry::ItemTypeRegistry()
    : d(std::make_unique<Private>())
{
}

ItemTypeRegistry::~ItemTypeRegistry() = default;

void ItemTypeRegistry::registerModule(const QString &moduleId, RegisterFunction fn)
{
    if (!isValidModuleId(moduleId))
        qFatal("Invalid module id \"%s\": use lowercase letters and digits only",
               qUtf8Printable(moduleId));
    if (!fn)
        qFatal("Module %s has no register function", qUtf8Printable(moduleId));
    // The registrar a module's register function sees. It knows the module id,
    // so it can check every type id against it.
    class Registrar : public ModuleRegistrar
    {
    public:
        Registrar(Private &registry, const QString &id)
            : m_registry(registry)
            , m_moduleId(id)
        {
        }

        void addItemType(std::unique_ptr<ItemType> type) override
        {
            m_registry.add(m_moduleId, std::move(type));
        }

        void setModuleName(const QString &name) override { m_registry.modules[m_moduleId].name = name; }

        void setModuleDescription(const QString &description) override
        {
            m_registry.modules[m_moduleId].description = description;
        }

        void addSettingsScreen(std::unique_ptr<SettingsScreen> screen) override
        {
            m_registry.addScreen(m_moduleId, std::move(screen));
        }

        const ModuleSettings &settings() const override { return *m_registry.modules.at(m_moduleId).settings; }

        void setServerType(std::unique_ptr<ServerType> type) override
        {
            if (!type)
                qFatal("Module %s set a null server type", qUtf8Printable(m_moduleId));
            auto &server = m_registry.modules[m_moduleId].server;
            if (server)
                qFatal("Module %s set its server type twice", qUtf8Printable(m_moduleId));
            server = std::move(type);
        }

        void addProjectSetting(const Field &field) override
        {
            m_registry.addProjectSetting(m_moduleId, field);
        }

    private:
        Private &m_registry;
        QString m_moduleId;
    };

    Private::Module &module = d->modules[moduleId];
    if (!module.settings)
        module.settings = std::make_unique<Settings>(moduleId, &d->reader);
    Registrar registrar(*d, moduleId);
    fn(registrar);
}

const ItemType &ItemTypeRegistry::type(const QString &id) const
{
    const auto found = d->byId.find(id);
    if (found != d->byId.end())
        return *found->second;
    auto &slot = d->unknown[id];
    if (!slot)
        slot = std::make_unique<UnknownItemType>(id);
    return *slot;
}

bool ItemTypeRegistry::contains(const QString &id) const
{
    return d->byId.count(id) != 0;
}

std::vector<const ItemType *> ItemTypeRegistry::types() const
{
    std::vector<const ItemType *> result;
    result.reserve(d->types.size());
    for (const auto &type : d->types)
        result.push_back(type.get());
    std::sort(result.begin(), result.end(), [](const ItemType *a, const ItemType *b) {
        const int byName = a->name().compare(b->name(), Qt::CaseInsensitive);
        if (byName != 0)
            return byName < 0;
        return a->id() < b->id();
    });
    return result;
}

bool ItemTypeRegistry::usesProfile(const QString &typeId) const
{
    if (!contains(typeId))
        return false;
    const ItemType &itemType = type(typeId);
    return itemType.rendering() == Rendering::Web || !settingsScreens(typeId.section(QLatin1Char('.'), 0, 0)).empty();
}

void ItemTypeRegistry::setSettingsReader(SettingsReader reader)
{
    d->reader = std::move(reader);
}

QStringList ItemTypeRegistry::moduleIds() const
{
    QStringList ids;
    for (const auto &entry : d->modules)
        ids << entry.first;
    return ids;
}

QString ItemTypeRegistry::moduleName(const QString &moduleId) const
{
    const auto found = d->modules.find(moduleId);
    if (found == d->modules.end() || found->second.name.isEmpty())
        return moduleId;
    return found->second.name;
}

QString ItemTypeRegistry::moduleDescription(const QString &moduleId) const
{
    const auto found = d->modules.find(moduleId);
    return found == d->modules.end() ? QString() : found->second.description;
}

std::vector<const ItemType *> ItemTypeRegistry::moduleTypes(const QString &moduleId) const
{
    std::vector<const ItemType *> result;
    for (const ItemType *type : types()) {
        if (idBelongsToModule(type->id(), moduleId))
            result.push_back(type);
    }
    return result;
}

std::vector<const SettingsScreen *> ItemTypeRegistry::settingsScreens(const QString &moduleId) const
{
    std::vector<const SettingsScreen *> screens;
    const auto found = d->modules.find(moduleId);
    if (found == d->modules.end())
        return screens;
    for (const auto &screen : found->second.screens)
        screens.push_back(screen.get());
    return screens;
}

const SettingsScreen *ItemTypeRegistry::settingsScreen(const QString &moduleId, const QString &screenId) const
{
    for (const SettingsScreen *screen : settingsScreens(moduleId)) {
        if (screen->id() == screenId)
            return screen;
    }
    return nullptr;
}

const ModuleSettings *ItemTypeRegistry::moduleSettings(const QString &moduleId) const
{
    const auto found = d->modules.find(moduleId);
    return found == d->modules.end() ? nullptr : found->second.settings.get();
}

const ItemType *ItemTypeRegistry::findType(const QString &typeId) const
{
    const auto found = d->byId.find(typeId);
    return found == d->byId.end() ? nullptr : found->second;
}

const ServerType *ItemTypeRegistry::serverType(const QString &moduleId) const
{
    const auto found = d->modules.find(moduleId);
    return found == d->modules.end() ? nullptr : found->second.server.get();
}

std::vector<Field> ItemTypeRegistry::projectSettings(const QString &moduleId) const
{
    const auto found = d->modules.find(moduleId);
    return found == d->modules.end() ? std::vector<Field>() : found->second.projectSettings;
}

QStringList ItemTypeRegistry::projectSettingModules() const
{
    QStringList ids;
    for (const auto &entry : d->modules) {
        if (!entry.second.projectSettings.empty())
            ids << entry.first;
    }
    return ids;
}

} // namespace mirante
