// The notes module: Markdown notes as an item. See README.md.
#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QCoreApplication>

#include <memory>

namespace mirante::modules::notes {

namespace {

// Placeholder type for M2: only an id, a name and an icon. Views arrive later.
class StubItemType : public ItemType
{
public:
    StubItemType(const QString &id, const QString &name)
        : m_id(id)
        , m_name(name)
        , m_icon(QStringLiteral(":/modules/notes/icons/notes.svg"))
    {
    }

    QString id() const override { return m_id; }
    QString name() const override { return m_name; }
    QIcon icon() const override { return m_icon; }
    QString shortName() const override { return QCoreApplication::translate("notes", "Notes"); }
    QString description() const override
    {
        return QCoreApplication::translate("notes", "A Markdown note with a preview. Its text is kept in the task's file.");
    }
    Rendering rendering() const override { return Rendering::Native; }

private:
    QString m_id;
    QString m_name;
    QIcon m_icon;
};

} // namespace

void registerModule(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QStringLiteral("Notes"));
    registrar.setModuleDescription(QCoreApplication::translate(
        "notes", "Markdown notes, kept in the task's file, so they sync with the workspace."));
    registrar.addItemType(std::make_unique<StubItemType>(
        QStringLiteral("notes"), QCoreApplication::translate("notes", "Notes")));
}

} // namespace mirante::modules::notes
