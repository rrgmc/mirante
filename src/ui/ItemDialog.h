#pragma once

#include "core/Entities.h"

#include <mirante/ItemEditor.h>

#include <QDialog>

#include <memory>

namespace mirante {

class ItemTypeRegistry;

// Creates or edits an item: its type (only when creating), its name, its web
// profile (only for a type whose views are web views), and its configuration, edited by the type's own editor or else a FieldForm. Changing
// the type while creating replaces the editor and starts from the new type's
// defaults. An item of a missing module shows its name only and keeps its
// configuration. OK is refused while the name is empty or the editor reports
// problems. See the M3 plan, part 5. Implemented in ItemDialog.cpp.
class ItemDialog : public QDialog
{
    Q_OBJECT

public:
    // creating: whether item is new, so its type can be chosen. An empty type
    // in a new item means the first type of the registry.
    ItemDialog(const Item &item, bool creating, const ItemTypeRegistry &types, EditorContext context,
               QWidget *parent = nullptr);
    ~ItemDialog() override;

    // The item with the edited fields; every other field as given.
    Item value() const;

    // The web profiles offered, from webprofiles::all(), after the first
    // entry, "The group's profile", which names groupProfileName. Only that
    // entry by default.
    void setWebProfiles(const std::vector<WebProfile> &profiles, const QString &groupProfileName);

    // For tests.
    void setWebProfile(const QString &id);   // empty for the group's profile
    bool showsWebProfile() const;
    void setType(const QString &typeId);
    void setName(const QString &name);
    ItemEditor *editor() const;     // null for an item of a missing module

    void accept() override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
