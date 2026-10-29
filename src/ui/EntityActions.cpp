#include "ui/EntityActions.h"

#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "core/Store.h"
#include "items/ItemTypeRegistry.h"

#include <QDialog>
#include <QMenu>

// Stubs written by T0 of the M3 plan. T12 implements them.

namespace mirante {

struct EntityActions::Private
{
};

EntityActions::EntityActions(Model &, Store &, const ItemTypeRegistry &, DeviceSettings &, QWidget *)
    : d(std::make_unique<Private>())
{
}

EntityActions::~EntityActions() = default;

void EntityActions::setExec(std::function<bool(QDialog &)>) {}
void EntityActions::setConfirm(std::function<bool(const QString &)>) {}

QString EntityActions::newGroup() { return {}; }
QString EntityActions::newProject(const QString &) { return {}; }
QString EntityActions::newTask(const QString &) { return {}; }
QString EntityActions::newItem(const QString &) { return {}; }
QString EntityActions::newDashboard(const QString &) { return {}; }

void EntityActions::editGroup(const QString &) {}
void EntityActions::editProject(const QString &) {}
void EntityActions::editTask(const QString &) {}
void EntityActions::editItem(const QString &, const QString &) {}
void EntityActions::renameDashboard(const QString &, const QString &) {}

void EntityActions::removeGroup(const QString &) {}
void EntityActions::removeProject(const QString &) {}
void EntityActions::removeTask(const QString &) {}
void EntityActions::removeItem(const QString &, const QString &) {}
void EntityActions::removeDashboard(const QString &, const QString &) {}

void EntityActions::setPriority(const QString &, Priority) {}
void EntityActions::setState(const QString &, TaskState) {}
void EntityActions::setDefaultDashboard(const QString &, const QString &) {}

void EntityActions::moveGroup(const QString &, int) {}
void EntityActions::moveProject(const QString &, Priority, int) {}
void EntityActions::moveTask(const QString &, Priority, int) {}

void EntityActions::fillMenu(QMenu *, SidebarModel::Kind, const QString &, const QString &) {}

} // namespace mirante
