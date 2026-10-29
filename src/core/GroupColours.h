#pragma once

#include "core/Entities.h"

#include <QString>
#include <QStringList>

namespace mirante {

class Model;

// The colour of each group, as "#rrggbb" strings, so the core stays free of
// Qt GUI. See docs/data-model.md#group and
// docs/decisions/0046-group-colours.md. Implemented in GroupColours.cpp.
namespace groupcolours {

// The fixed palette of calm colours, in the order new groups take them. It
// leaves out red, orange and grey, which the sidebar uses for priorities.
const QStringList &palette();

// Whether text is a colour that can be stored: "#rrggbb", in either case.
bool isValid(const QString &text);

// The palette colour of a group without a colour of its own. It comes from a
// hash of the id that is the same on every device and every run.
QString fallback(const QString &groupId);

// The group's colour: its own if valid, otherwise fallback(). Lower case.
QString of(const Group &group);

// The colour of the task's group, or an empty string if the task, its
// project or its group is unknown.
QString ofTask(const Model &model, const QString &taskId);

// The colour for a new group: the first palette colour that no group uses,
// counting the fallback colours of groups without one. If every colour is
// used, the one used least, the earliest in the palette on a tie.
QString next(const Model &model);

} // namespace groupcolours
} // namespace mirante
