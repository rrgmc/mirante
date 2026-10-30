// The notes module: Markdown notes as an item. See README.md.
#include "NotesView.h"

#include <mirante/ItemContext.h>
#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QCoreApplication>

#include <memory>

namespace mirante::modules::notes {

namespace {

// The Notes type. Its text is in the configuration under "text", which the
// item dialog does not show, so it keeps it.
class NotesItemType : public ItemType
{
public:
    NotesItemType(const QString &id, const QString &name)
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
        return QCoreApplication::translate("notes",
                                           "A Markdown note with a preview. Its text is kept in the task's file.");
    }
    Rendering rendering() const override { return Rendering::Native; }

    // The view saves the text as the user types. See decision 0081.
    QStringList viewKeys() const override { return {QStringLiteral("text")}; }

    QWidget *createView(ItemContext &context, QWidget *parent) const override
    {
        return createNotesView(context, parent);
    }

private:
    QString m_id;
    QString m_name;
    QIcon m_icon;
};

} // namespace

void registerModule(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QCoreApplication::translate("notes", "Notes"));
    registrar.setModuleDescription(QCoreApplication::translate(
        "notes", "Markdown notes, kept in the task's file, so they sync with the workspace."));
    registrar.addItemType(std::make_unique<NotesItemType>(
        QStringLiteral("notes"), QCoreApplication::translate("notes", "Notes")));
}

} // namespace mirante::modules::notes
