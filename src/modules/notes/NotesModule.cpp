// The notes module: Markdown notes and lists of links as items. See README.md.
#include "Links.h"
#include "LinksEditor.h"
#include "LinksView.h"
#include "NotesView.h"

#include <mirante/ItemContext.h>
#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QCoreApplication>
#include <QUrl>
#include <QUuid>

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

// The Links type. Its links are in the configuration under "links", which its
// editor writes. See docs/item-types.md#links.
class LinksItemType : public ItemType
{
public:
    QString id() const override { return QStringLiteral("notes.links"); }
    QString name() const override { return QCoreApplication::translate("notes", "Links"); }
    QIcon icon() const override { return QIcon(QStringLiteral(":/modules/notes/icons/links.svg")); }
    QString shortName() const override { return QCoreApplication::translate("notes", "Links"); }
    QString description() const override
    {
        return QCoreApplication::translate("notes",
                                           "A list of links with titles and short descriptions. A link leads to a web "
                                           "address, a file in a project folder or a file in the workspace.");
    }
    Rendering rendering() const override { return Rendering::Native; }

    ItemEditor *createEditor(const EditorContext &context, QWidget *parent) const override
    {
        return new LinksEditor(context, parent);
    }

    QWidget *createView(ItemContext &context, QWidget *parent) const override
    {
        if (linksOf(context.config()).isEmpty())
            return context.createMessageView(
                QCoreApplication::translate("notes", "Add links in the item's settings."), parent);
        return createLinksView(context, parent);
    }
};

} // namespace

void registerModule(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QCoreApplication::translate("notes", "Notes"));
    registrar.setModuleDescription(QCoreApplication::translate(
        "notes", "Markdown notes and lists of links, kept in the task's file, so they sync with the workspace."));
    registrar.setModuleUuid(QUuid(QStringLiteral("27bc56ed-94f4-43f0-85be-623c723b1bb0")));
    registrar.setModuleVersion(QStringLiteral("0.1.0"));
    registrar.setModuleAuthor(QStringLiteral("Rangel Reale"));
    registrar.setModuleHomepage(QUrl(QStringLiteral("https://github.com/rrgmc/mirante/tree/master/src/modules/notes")));
    registrar.addItemType(std::make_unique<NotesItemType>(
        QStringLiteral("notes"), QCoreApplication::translate("notes", "Notes")));
    registrar.addItemType(std::make_unique<LinksItemType>());
}

} // namespace mirante::modules::notes
