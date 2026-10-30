#pragma once

#include "core/Entities.h"

#include <QString>

#include <vector>

namespace mirante {

class Model;

// The workspace's profiles and the profile each item uses. A profile holds
// web logins and module settings. See docs/data-model.md#profile and
// decisions 0050, 0054 and 0057 in docs/decisions/.
// Implemented in Profiles.cpp.
namespace profiles {

// The name of the profile a workspace gets when it has none: "Default".
QString defaultName();

// Whether id names a profile the workspace lists.
bool exists(const Model &model, const QString &id);

// Every profile to offer: the default first, then the others by name,
// ignoring case.
std::vector<Profile> all(const Model &model);

// The name to show in lists: the default gets " (default)" after its name.
QString listName(const Model &model, const Profile &profile);

// The name to show for a profile id. An unknown id gives an empty string.
QString nameOf(const Model &model, const QString &id);

// The group's profile, or the workspace's default if it names none that exists.
QString groupDefault(const Model &model, const Group &group);

// The profile of an item, by the resolution order: the item's own profile,
// else its group's, else the workspace's default. A step that names a profile that
// does not exist, because it was deleted or has not arrived yet, is skipped.
// group may be null.
QString effective(const Model &model, const Item &item, const Group *group);
// The same for an item of a task. The default if the item is unknown.
QString effective(const Model &model, const QString &taskId, const QString &itemId);
// The profile an item of the task would use if its own profile were
// itemProfile. Used while an item is edited, before it is saved.
QString effectiveFor(const Model &model, const QString &taskId, const QString &itemProfile);

// Removes a profile from the workspace's list, with its module settings.
// Groups and items that name it are left as they are, and fall back by the
// order above. Removing the default makes the first of the others by name
// the default. Returns false for an unknown id or the last profile.
bool remove(Model &model, const QString &id);

} // namespace profiles
} // namespace mirante
