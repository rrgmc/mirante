#pragma once

#include "core/Entities.h"
#include "ui/SidebarModel.h"

#include <QObject>
#include <QUrl>

#include <functional>
#include <memory>

class QDialog;
class QImage;
class QMenu;
class QWidget;

namespace mirante {

class DeviceSettings;
class ItemTypeRegistry;
class Model;
class ProfileManager;
class SecretStore;
class Store;
class ProfilesDialog;

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
    // Reads the keyboard modifiers when a menu command runs. The default is
    // QGuiApplication::queryKeyboardModifiers(); tests set their own.
    void setModifiers(std::function<Qt::KeyboardModifiers()> modifiers);
    // The profiles' web logins. Deleting a profile then deletes its logins on
    // this device too. Null, the default, leaves them alone. The manager must
    // outlive this object.
    void setProfileManager(ProfileManager *manager);
    // The tokens of this device, kept under secretKey() of SecretStore.h.
    // Tokens typed into a settings screen are written there, a token's Remove
    // button removes it, and deleting a profile removes its tokens. Settings
    // screens show whether each token is set. Null, the default, drops typed
    // tokens. It must outlive this object. See the M5 plan, part 8, and
    // decision 0054.
    void setSecretStore(SecretStore *store);
    // Runs the profiles dialog. The default calls exec(); tests work the
    // dialog and return instead. The confirmation set with setConfirm() is
    // used.
    void setEditProfiles(std::function<void(ProfilesDialog &dialog)> edit);

    // Opens the profiles dialog, Profiles > Manage Profiles…. See docs/ui.md#profiles.
    void editProfiles();

    // Opens the dialog of a module's settings screen in one profile, from
    // Profiles > <profile> > Modules, and stores the values in the model.
    // Tokens typed into it go to setSecretStore(). Nothing happens for an
    // unknown screen or profile. See docs/ui.md#modules.
    void editModuleSettings(const QString &profileId, const QString &moduleId, const QString &screenId);

    // Creating. Each opens the entity's dialog with a new value, and returns the
    // new id, or an empty string if the dialog was cancelled. A new task gets one
    // dashboard with one empty cell. A new dashboard is built from the chosen preset.
    QString newGroup();
    QString newProject(const QString &groupId);
    QString newTask(const QString &projectId);
    // url fills the link field of the item dialog's type page. The types read
    // the link with their module settings in the group's profile. See
    // decisions 0049 and 0054.
    QString newItem(const QString &taskId, const QString &url = QString());
    // "Paste URL…": newItem() with the web address on the clipboard. Nothing
    // happens, and an empty string is returned, if the clipboard holds none.
    QString pasteUrl(const QString &taskId);
    // The web address on the clipboard: its first http or https URL, else its
    // text if that is one. Empty if it holds none.
    static QUrl clipboardUrl();
    QString newDashboard(const QString &taskId);
    // "Paste Image…": opens the paste dialog on the clipboard's image, then
    // adds the cropped image to the task as a new Image item, on no
    // dashboard. A paste that cannot happen, such as in the read-only sample
    // data, emits message() instead of opening the dialog. Returns the new
    // item's id, or an empty string. See docs/decisions/0051-paste-images-from-the-items-menu.md.
    QString pasteImage(const QString &taskId);
    // pasteImage() with a given image, such as one dropped from another app
    // on a task or its Items row in the sidebar. See
    // docs/decisions/0085-drop-images-from-other-apps.md.
    QString addImage(const QString &taskId, const QImage &image);
    // Reads the clipboard's image for pasteImage() and its menu entry. The
    // default reads the system clipboard; tests set their own.
    void setClipboardImage(std::function<QImage()> clipboardImage);

    // Editing, through the entity's dialog.
    void editGroup(const QString &id);
    void editProject(const QString &id);
    void editTask(const QString &id);
    void editItem(const QString &taskId, const QString &itemId);
    void renameDashboard(const QString &taskId, const QString &dashboardId);

    // The trash. See docs/ui.md#trash. Moving a project or task to the trash
    // sets its trashedAt, without a confirmation; a project takes its tasks
    // with it, without changing them. Restoring clears trashedAt. Restoring a
    // task whose project is in the trash restores the project too.
    void trashProject(const QString &id);
    void trashTask(const QString &id);
    void restoreProject(const QString &id);
    void restoreTask(const QString &id);
    // Deletes everything in the trash for good, after one confirmation that
    // names the counts. Nothing happens when the trash is empty.
    void emptyTrash();
    // What the trash holds: projects in the trash, and tasks in the trash,
    // counting every task of a project in the trash.
    int trashedProjectCount() const;
    int trashedTaskCount() const;

    // Deleting for good, after a confirmation that names what goes with the
    // entity, such as "Delete project backend and its 4 tasks?". The last
    // dashboard of a task cannot be deleted.
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
    void moveItem(const QString &taskId, const QString &itemId, int index);
    void moveDashboard(const QString &taskId, const QString &dashboardId, int index);

    // Adds the commands for a sidebar row to menu: new, edit, rename, set
    // priority, set state, delete, and on dashboards "Set as default". A task
    // and its Items row also get "Paste Image…", enabled while the clipboard
    // holds an image. Fits
    // Sidebar::MenuFiller. "Delete project…" and "Delete task…" move to the
    // trash, or delete for good while Shift is held.
    void fillMenu(QMenu *menu, SidebarModel::Kind kind, const QString &id, const QString &taskId);
    // The commands for a row of the sidebar's Trash list: "Restore" and
    // "Delete permanently…" on projects and tasks. Fits Sidebar::MenuFiller.
    void fillTrashMenu(QMenu *menu, SidebarModel::Kind kind, const QString &id, const QString &taskId);

signals:
    // A task dashboard made with newDashboard, so the window can show it.
    void dashboardCreated(const QString &taskId, const QString &dashboardId);
    // A short message for the status bar, such as why an image was not pasted.
    void message(const QString &text);

private:
    // Adds "Paste Image…" for the task, enabled while the clipboard holds an image.
    void addPasteImage(QMenu *menu, const QString &taskId);

    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
