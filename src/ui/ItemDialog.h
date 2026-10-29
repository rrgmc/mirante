#pragma once

#include "core/Entities.h"

#include <mirante/ItemEditor.h>

#include <QDialog>

#include <memory>

namespace mirante {

class ItemTypeRegistry;

// Creates or edits an item.
//
// Creating takes two steps on two pages of one dialog. The first page picks
// the type in a grid of large icons, with Next, a double click or Enter going
// on. The second page edits the item of that type, with Back to choose again.
// Choosing another type replaces the editor and starts from the new type's
// defaults; choosing the same one keeps what was entered. Editing an existing
// item shows the second page only, and its type cannot change.
//
// Above the grid, a link field takes a pasted web address. While it holds
// one, the grid shows only the types whose fromUrl() takes it, and choosing
// one starts from the configuration and name it made. See decision 0049.
//
// The second page holds the name, the web profile (only for a type whose views
// are web views), and the configuration, edited by the type's own editor or
// else a FieldForm. An item of a missing module shows its name only and keeps
// its configuration. OK is refused while the name is empty or the editor
// reports problems. See docs/ui.md#item-dialog and decision 0043.
// Implemented in ItemDialog.cpp.
class ItemDialog : public QDialog
{
    Q_OBJECT

public:
    // creating: whether item is new, so its type is chosen first. The type
    // grid starts with the item's type selected, or else the first type of
    // the registry, by name.
    ItemDialog(const Item &item, bool creating, const ItemTypeRegistry &types, EditorContext context,
               QWidget *parent = nullptr);
    ~ItemDialog() override;

    // The item with the edited fields; every other field as given.
    Item value() const;

    // The web profiles offered, from webprofiles::all(), after the first
    // entry, "The group's profile", which names groupProfileName. Only that
    // entry by default.
    void setWebProfiles(const std::vector<WebProfile> &profiles, const QString &groupProfileName);

    // The connections of the new item's group. A pasted link is read with
    // the base URLs of those of each type's kinds, and an item made from a
    // link uses the connection of its server. None by default.
    void setConnections(const std::vector<Connection> &connections);
    // Fills the link field of the type page, as pasting into it does. The
    // grid then shows only the types that take the link, best first, and
    // choosing one fills its configuration and name. See decision 0049.
    void setUrl(const QString &text);
    // The id of the group's connection whose base URL the chosen type's link
    // names. Empty when the type was not chosen from a link, or no connection
    // of the group matches.
    QString urlConnectionId() const;

    // For tests.
    void setWebProfile(const QString &id);   // empty for the group's profile
    bool showsWebProfile() const;
    // Chooses a registered type while creating, as the type page does, and
    // shows the item's page. Does nothing while editing.
    void setType(const QString &typeId);
    // Whether the type page shows.
    bool choosingType() const;
    // Goes back to the type page while creating.
    void back();
    void setName(const QString &name);
    ItemEditor *editor() const;     // null for an item of a missing module

    // On the type page, chooses the selected type instead of accepting.
    void accept() override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
