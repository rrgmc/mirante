#pragma once

#include "core/Entities.h"
#include "core/LayoutTree.h"

#include <QJsonObject>
#include <QObject>

#include <memory>
#include <vector>

namespace mirante {

// Names one dashboard: a task dashboard (taskId and dashboardId), or a root
// dashboard (dashboardId only).
struct DashboardRef
{
    QString taskId;
    QString dashboardId;

    bool isRoot() const { return taskId.isEmpty(); }

    friend bool operator==(const DashboardRef &a, const DashboardRef &b)
    {
        return a.taskId == b.taskId && a.dashboardId == b.dashboardId;
    }
    friend bool operator!=(const DashboardRef &a, const DashboardRef &b) { return !(a == b); }
};

// Owns all entities. All changes go through its methods, and each change emits
// a signal. See docs/architecture.md#core. Implemented in Model.cpp.
//
// Pointers returned by lookups stay valid until the next change that removes
// or replaces that entity, or the next reset().
class Model : public QObject
{
    Q_OBJECT

public:
    explicit Model(QObject *parent = nullptr);
    ~Model() override;

    // Replaces everything. Used by the Store after loading. Emits modelReset().
    void reset(std::vector<Group> groups, std::vector<Project> projects, std::vector<Task> tasks,
               std::vector<RootDashboard> rootDashboards);

    // Lookups. Each returns nullptr if the id is unknown.
    const Group *group(const QString &id) const;
    const Project *project(const QString &id) const;
    const Task *task(const QString &id) const;
    const Item *item(const QString &taskId, const QString &itemId) const;
    const RootDashboard *rootDashboard(const QString &id) const;
    const Dashboard *taskDashboard(const QString &taskId, const QString &dashboardId) const;
    const LayoutNode *layout(const DashboardRef &dashboard) const;
    const Task *taskOfItem(const QString &itemId) const;

    // Children, unsorted. Use Sorting for display order.
    std::vector<const Group *> groups() const;
    std::vector<const Project *> projectsOfGroup(const QString &groupId) const;
    std::vector<const Task *> tasksOfProject(const QString &projectId) const;
    std::vector<const RootDashboard *> rootDashboards() const;

    // The workspace's name, from workspace.mirante in the data folder. Empty if
    // it has none. reset() clears it.
    QString workspaceName() const;
    void setWorkspaceName(const QString &name);

    // The workspace's id, from workspace.mirante: a UUID that stays when the
    // folder moves. This device keeps the workspace's web logins under it.
    // Empty for a workspace from before decision 0059 until the Store gives
    // it one. reset() clears it. See docs/decisions/0059-logins-per-workspace.md.
    QString workspaceId() const;
    void setWorkspaceId(const QString &id);

    // The workspace's profiles, from workspace.mirante, in their stored
    // order. There is always at least one. See core/Profiles.h for lookups by
    // the resolution order. reset() leaves one empty profile called
    // "Default", with the id kDefaultProfile, which is the default.
    const std::vector<Profile> &profiles() const;
    const Profile *profile(const QString &id) const;   // nullptr if not listed
    // Replaces the list. An empty list gets the profile reset() leaves.
    void setProfiles(std::vector<Profile> profiles);
    // The id of the default profile, where the resolution order ends. Always
    // a listed profile: if the stored id is not listed, the first profile by
    // name, ignoring case. See docs/decisions/0057-any-profile-can-be-the-default.md.
    QString defaultProfile() const;
    // Stores the id of the default profile. An id that is not listed is kept,
    // and defaultProfile() falls back until it is.
    void setDefaultProfile(const QString &id);
    // Adds the profile, or replaces the one with its id.
    void setProfile(const Profile &profile);

    // The values of one module settings screen in a profile. Empty if none
    // are stored or the profile is unknown. See
    // docs/decisions/0050-profiles-hold-logins-connections-and-settings.md.
    QJsonObject moduleSettings(const QString &profileId, const QString &moduleId, const QString &screenId) const;
    // Replaces the values of one screen in a profile. Empty values remove the
    // screen's entry, and a module left without screens is removed too.
    // Nothing happens for an unknown profile.
    void setModuleSettings(const QString &profileId, const QString &moduleId, const QString &screenId,
                           const QJsonObject &values);

    // Whether a task is in the trash: its own trashedAt is set, or its
    // project's is. See docs/data-model.md#task.
    bool taskInTrash(const Task &task) const;
    bool taskInTrash(const QString &taskId) const;   // false for an unknown task

    // Whether a cell reference still refers to something. Used for normalisation.
    // Entities in the trash still exist, so references to them are kept.
    bool refExists(const CellRef &ref) const;

    // Changes from M3. See the M3 plan, part 1. Every UI edit copies an entity,
    // changes the copy and calls a setter; the Store's reload calls the same
    // setters. A setter adds the entity, or replaces the one with the same id,
    // and emits the added or changed signal. A value equal to the stored one
    // emits nothing.
    void setGroup(const Group &group);
    void setProject(const Project &project);   // a new groupId moves the project
    void setTask(const Task &task);            // normalises its dashboards; a new projectId moves it
    void setRootDashboard(const RootDashboard &dashboard);   // normalises its layout
    // Replaces the item with the same id in its task, through setTask().
    // Nothing happens if the task does not hold it.
    void setItem(const QString &taskId, const Item &item);
    // Removers. Each emits the removed signal for every entity it removes,
    // children first. Root dashboards that show a removed task are not changed:
    // their cells show "Removed" through refExists().
    void removeGroup(const QString &id);       // and its projects and their tasks
    void removeProject(const QString &id);     // and its tasks
    void removeTask(const QString &id);

    // Changes in M2. Each normalises the tree before storing it.
    void setTaskDashboardLayout(const QString &taskId, const QString &dashboardId, const LayoutNode &tree);
    void setRootDashboardLayout(const QString &id, const LayoutNode &tree);
    void setLayout(const DashboardRef &dashboard, const LayoutNode &tree);   // calls one of the two above
    void addRootDashboard(const RootDashboard &dashboard);
    // Deletes a root dashboard. Closing its tab does not; this does.
    void removeRootDashboard(const QString &id);

signals:
    void modelReset();
    void workspaceNameChanged();
    void profilesChanged();
    void groupAdded(const QString &id);
    void groupChanged(const QString &id);
    void groupRemoved(const QString &id);
    void projectAdded(const QString &id);
    void projectChanged(const QString &id);
    void projectRemoved(const QString &id);
    void taskAdded(const QString &taskId);
    void taskChanged(const QString &taskId);
    void taskRemoved(const QString &taskId);
    void rootDashboardChanged(const QString &id);
    void rootDashboardAdded(const QString &id);
    void rootDashboardRemoved(const QString &id);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
