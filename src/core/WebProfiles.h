#pragma once

#include "core/Entities.h"

#include <QString>

#include <vector>

namespace mirante {

class Model;

// The workspace's web profiles and the profile each item uses. See
// docs/web-views.md#profiles. Implemented in WebProfiles.cpp.
namespace webprofiles {

// Whether id names a profile: Default, or one the workspace lists.
bool exists(const Model &model, const QString &id);

// Every profile to offer: Default first, then the workspace's profiles by
// name, ignoring case.
std::vector<WebProfile> all(const Model &model);

// The name to show for a profile id. An unknown id gives an empty string.
QString nameOf(const Model &model, const QString &id);

// The group's default profile, or Default if it names none that exists.
QString groupDefault(const Model &model, const Group &group);

// The profile of an item, by the resolution order: the item's own profile,
// else its group's default, else Default. A step that names a profile that
// does not exist, because it was deleted or has not arrived yet, is skipped.
// group may be null.
QString effective(const Model &model, const Item &item, const Group *group);
// The same for an item of a task. Default if the item is unknown.
QString effective(const Model &model, const QString &taskId, const QString &itemId);

// Removes a profile from the workspace's list. Groups and items that name it
// are left as they are, and fall back by the order above. Default cannot be
// removed. Returns false for Default or an unknown id.
bool remove(Model &model, const QString &id);

} // namespace webprofiles
} // namespace mirante
