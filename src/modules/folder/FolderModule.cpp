// The folder module: a project folder as an item. See README.md.
#include <mirante/Fields.h>
#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QCoreApplication>

#include <memory>
#include <vector>

namespace mirante::modules::folder {

namespace {

// A type with an id, a name, an icon and its configuration fields. The view arrives later.
class StubItemType : public ItemType
{
public:
    StubItemType(const QString &id, const QString &name)
        : m_id(id)
        , m_name(name)
        , m_icon(QStringLiteral(":/modules/folder/icons/folder.svg"))
    {
    }

    QString id() const override { return m_id; }
    QString name() const override { return m_name; }
    QIcon icon() const override { return m_icon; }
    QString shortName() const override { return QCoreApplication::translate("folder", "Folder"); }
    QString description() const override
    {
        return QCoreApplication::translate("folder", "One of the project's folders: its local path, Git branch and status, with "
                                               "shortcuts to a terminal, an editor and the file manager.");
    }
    Rendering rendering() const override { return Rendering::Native; }

    std::vector<Field> fields() const override
    {
        Field folder;
        folder.key = QStringLiteral("folder");
        folder.label = QCoreApplication::translate("folder", "Folder");
        folder.type = Field::Type::Folder;
        folder.required = true;
        return {folder};
    }

private:
    QString m_id;
    QString m_name;
    QIcon m_icon;
};

} // namespace

void registerModule(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QStringLiteral("Folder"));
    registrar.setModuleDescription(QCoreApplication::translate(
        "folder", "A project's folder as an item, with its Git state and shortcuts to a terminal and an editor."));
    registrar.addItemType(std::make_unique<StubItemType>(
        QStringLiteral("folder"), QCoreApplication::translate("folder", "Folder / terminal")));
}

} // namespace mirante::modules::folder
