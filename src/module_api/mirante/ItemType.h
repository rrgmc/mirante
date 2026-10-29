#pragma once

#include <mirante/Fields.h>

#include <QIcon>
#include <QString>

#include <vector>

class QWidget;

namespace mirante {

class ItemEditor;
struct EditorContext;

// One kind of item, such as a GitLab merge request summary. Modules register
// their types through ModuleRegistrar. See docs/item-types.md.
//
// createView() and ItemContext arrive in M4.
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
};

} // namespace mirante
