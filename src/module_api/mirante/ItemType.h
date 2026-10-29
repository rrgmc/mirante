#pragma once

#include <mirante/Fields.h>
#include <mirante/UrlMatch.h>

#include <QIcon>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <optional>
#include <vector>

class QWidget;

namespace mirante {

class ItemContext;
class ItemEditor;
struct EditorContext;

// How a type's views are made. Web views are the heavy ones that the memory
// policy unloads. See docs/web-views.md#memory-policy.
enum class Rendering { Native, Web };

// One kind of item, such as a GitLab merge request summary. Modules register
// their types through ModuleRegistrar. See docs/item-types.md.
class ItemType
{
public:
    virtual ~ItemType() = default;

    // The id, starting with the module id and a dot, such as "gitlab.mr.summary".
    // A module with a single type may use the module id itself, such as "notes".
    virtual QString id() const = 0;

    // The name shown to the user, such as "GitLab MR (summary)".
    virtual QString name() const = 0;

    virtual QIcon icon() const = 0;

    // A short label for the type, one word if possible, such as "MR" or "Jira".
    // Cell headers show it in a small pill with the icon. Defaults to name().
    virtual QString shortName() const { return name(); }

    // True if only one cell may show the item at a time. See docs/item-types.md#the-view.
    virtual bool singleView() const { return false; }

    // False if the view must be made again instead of being moved to another parent.
    virtual bool reparentSafe() const { return true; }

    // How the configuration is edited: either a list of fields, from which the
    // core builds a form, or an editor of the type's own. See
    // docs/decisions/0028-item-fields-in-m3.md.
    virtual std::vector<Field> fields() const { return {}; }
    // The fields for an item that uses the profile with this id, such as a
    // field whose suggestions come from that profile's module settings. The
    // item dialog calls it with the profile the edited item would use.
    // Defaults to fields(). See decision 0050.
    virtual std::vector<Field> fieldsForProfile(const QString &profileId) const
    {
        Q_UNUSED(profileId)
        return fields();
    }
    // An editor of the type's own, or null to use a form built from fields().
    // Include <mirante/ItemEditor.h> to implement it.
    virtual ItemEditor *createEditor(const EditorContext &context, QWidget *parent) const
    {
        Q_UNUSED(context)
        Q_UNUSED(parent)
        return nullptr;
    }

    // The key of an Image field that a pasted image fills. If set, pasting an
    // image onto a task dashboard creates an item of this type. See
    // docs/item-types.md#image.
    virtual QString pastedImageField() const { return {}; }

    // Recognises a web address that the user pasted, such as a merge
    // request's page, and returns the configuration that shows it. Null if
    // the type does not take the address, the default. profileId is the
    // profile the new item would use, so a type can read the server
    // configured in its module settings there, such as a self-hosted GitLab.
    // See docs/modules.md#items-from-a-link and decision 0054.
    virtual std::optional<UrlMatch> fromUrl(const QUrl &url, const QString &profileId) const
    {
        Q_UNUSED(url)
        Q_UNUSED(profileId)
        return std::nullopt;
    }

    virtual Rendering rendering() const { return Rendering::Native; }

    // Creates the view for one placement. The context outlives the view.
    // Null shows the type's placeholder. Include <mirante/ItemContext.h> to
    // implement it. See docs/item-types.md#the-view.
    virtual QWidget *createView(ItemContext &context, QWidget *parent) const
    {
        Q_UNUSED(context)
        Q_UNUSED(parent)
        return nullptr;
    }
};

} // namespace mirante
