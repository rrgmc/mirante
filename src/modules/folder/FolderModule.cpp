// The folder module: a project folder as an item. See README.md.
#include "FolderView.h"

#include <mirante/Fields.h>
#include <mirante/ItemContext.h>
#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QCoreApplication>
#include <QUrl>
#include <QUuid>

#include <memory>
#include <vector>

namespace mirante::modules::folder {

namespace {

// The Folder / terminal type.
class FolderItemType : public ItemType
{
public:
    FolderItemType(const QString &id, const QString &name)
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
        return QCoreApplication::translate("folder",
                                           "One of the project's folders: its local path, Git branch and status, with "
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

    QWidget *createView(ItemContext &context, QWidget *parent) const override
    {
        return createFolderView(context, parent);
    }

private:
    QString m_id;
    QString m_name;
    QIcon m_icon;
};

} // namespace

void registerModule(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QCoreApplication::translate("folder", "Folder"));
    registrar.setModuleDescription(QCoreApplication::translate(
        "folder", "A project's folder as an item, with its Git state and shortcuts to a terminal and an editor."));
    registrar.setModuleUuid(QUuid(QStringLiteral("f28b2ac0-3992-4a4c-8e77-40b891218bbc")));
    registrar.setModuleVersion(QStringLiteral("0.1.0"));
    registrar.setModuleAuthor(QStringLiteral("Rangel Reale"));
    registrar.setModuleHomepage(QUrl(QStringLiteral("https://github.com/rrgmc/mirante/tree/master/src/modules/folder")));
    registrar.addItemType(std::make_unique<FolderItemType>(
        QStringLiteral("folder"), QCoreApplication::translate("folder", "Folder / terminal")));

    // The editor that opens a folder, kept on each device. The folder's path
    // is its last argument. See decision 0079.
    Field editor;
    editor.key = QStringLiteral("editor");
    editor.label = QCoreApplication::translate("folder", "Editor command");
    editor.defaultValue = QStringLiteral("code");
    editor.placeholder = QStringLiteral("code");
    registrar.addDeviceSetting(editor);
}

} // namespace mirante::modules::folder
