#pragma once

#include "ui/SidebarModel.h"

namespace mirante {

// Roles that only the sidebar's own model and delegate use.
enum SidebarInternalRole {
    // For tasks: true for the Default task, which has no marker and is never dimmed.
    SidebarDefaultTaskRole = SidebarModel::PriorityRole + 1,
};

} // namespace mirante
