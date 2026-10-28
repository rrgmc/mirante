#pragma once

#include <QIcon>
#include <QString>

namespace mirante {

// One kind of item, such as a GitLab merge request summary. Modules register
// their types through ModuleRegistrar. See docs/item-types.md.
//
// M2 holds only what the placeholders need. createView() and ItemContext
// arrive in M4.
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

    // True if only one cell may show the item at a time. See docs/item-types.md#the-view.
    virtual bool singleView() const { return false; }

    // False if the view must be made again instead of being moved to another parent.
    virtual bool reparentSafe() const { return true; }
};

} // namespace mirante
