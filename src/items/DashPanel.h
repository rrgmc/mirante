#pragma once

#include <QString>
#include <QStringList>

namespace mirante {

class ItemTypeRegistry;

// The built-in panels: the core's module "dash", with the one item type
// "dash.panel". See docs/item-types.md#built-in-panels and
// docs/decisions/0094-built-in-panels-as-a-core-item-type.md.
// Implemented in DashPanel.cpp.
namespace dash {

// "dash.panel".
QString panelTypeId();

// The panels, each a folder under dash://panels/, such as "task-summary".
QStringList panelIds();

// Registers the module "dash" as built in. main.cpp calls it before the
// enabled modules.
void registerModule(ItemTypeRegistry &registry);

} // namespace dash

} // namespace mirante
