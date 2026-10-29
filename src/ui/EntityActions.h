#pragma once

#include "core/Entities.h"
#include "ui/SidebarModel.h"

#include <QObject>

#include <functional>
#include <memory>

class QDialog;
class QMenu;
class QWidget;

namespace mirante {

class DeviceSettings;
class ItemTypeRegistry;
class Model;
class Store;

// Every command that creates, edits, moves or deletes a group, project, task,
// item or task dashboard. The sidebar menus, the menu bar and drops in the
// sidebar share them. Each opens its dialog, then applies the result through
// the model's setters. See the M3 plan, part 6. Implemented in EntityActions.cpp.
class EntityActions : public QObject
{
    Q_OBJECT

public:
    EntityActions(Model &model, Store &store, const ItemTypeRegistry &types, DeviceSettings &settings,
                  QWidget *dialogParent);
    ~EntityActions() override;

    // Runs a dialog and says whether it was accepted. The default calls
    // exec(); tests fill the dialog in and accept it instead.
    void setExec(std::function<bool(QDialog &dialog)> exec);
    // Asks the user to confirm a deletion. The default shows a message box.
    void setConfirm(std::function<bool(const QString &question)> confirm);

    // Creating. Each opens the entity's dialog with a new value, and returns the
    // new id, or an empty string if the dialog was cancelled. A new task gets one
    // dashboard with one empty cell. A new dashboard is built from the chosen preset.
    QString newGroup();
    QString newProject(const QString &groupId);
    QString newTask(const QString &projectId);
    QString newItem(const QString &taskId);
    QString newDashboard(const QString &taskId);

    // Editing, through the entity's dialog.
    void editGroup(const QString &id);
    void editProject(const QString &id);
    void editTask(const QString &id);
    void editItem(const QString &taskId, const QString &itemId);
    void renameDashboard(const QString &taskId, const QString &dashboardId);

    // Deleting, after a confirmation that names what goes with the entity, such
    // as "Delete project backend and its 4 tasks?". The last dashboard of a task
    // cannot be deleted.
    void removeGroup(const QString &id);
    void removeProject(const QString &id);
    void removeTask(const QString &id);
    void removeItem(const QString &taskId, const QString &itemId);
    void removeDashboard(const QString &taskId, const QString &dashboardId);

    // Quick changes, without a dialog.
    void setPriority(const QString &projectOrTaskId, Priority priority);
    void setState(const QString &taskId, TaskState state);
    void setDefaultDashboard(const QString &taskId, const QString &dashboardId);

    // Moves from sidebar drops; see SidebarModel::moveRequested.
    void moveGroup(const QString &id, int index);
    void moveProject(const QString &id, int index);
    void moveTask(const QString &id, int index);

    // Adds the commands for a sidebar row to menu: new, edit, rename, set
    // priority, set state, delete, and on dashboards "Set as default". Fits
    // Sidebar::MenuFiller.
    void fillMenu(QMenu *menu, SidebarModel::Kind kind, const QString &id, const QString &taskId);

signals:
    // A task dashboard made with newDashboard, so the window can show it.
    void dashboardCreated(const QString &taskId, const QString &dashboardId);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
