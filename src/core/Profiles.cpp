#include "core/Profiles.h"

#include "core/Model.h"

#include <QCoreApplication>

#include <algorithm>

namespace mirante::profiles {

namespace {

QString defaultName() { return QCoreApplication::translate("Profiles", "Default"); }

const Group *groupOfTask(const Model &model, const QString &taskId)
{
    const Task *task = model.task(taskId);
    const Project *project = task ? model.project(task->projectId) : nullptr;
    return project ? model.group(project->groupId) : nullptr;
}

} // namespace

bool exists(const Model &model, const QString &id)
{
    return !id.isEmpty() && model.profile(id);
}

std::vector<Profile> all(const Model &model)
{
    std::vector<Profile> listed;
    Profile standard;
    for (const Profile &profile : model.profiles()) {
        if (profile.id == kDefaultProfile)
            standard = profile;
        else
            listed.push_back(profile);
    }
    std::stable_sort(listed.begin(), listed.end(), [](const Profile &a, const Profile &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    standard.id = kDefaultProfile;
    standard.name = defaultName();
    listed.insert(listed.begin(), standard);
    return listed;
}

QString nameOf(const Model &model, const QString &id)
{
    if (id == kDefaultProfile)
        return defaultName();
    const Profile *profile = model.profile(id);
    return profile ? profile->name : QString();
}

QString groupDefault(const Model &model, const Group &group)
{
    return exists(model, group.profile) ? group.profile : kDefaultProfile;
}

QString effective(const Model &model, const Item &item, const Group *group)
{
    if (exists(model, item.profile))
        return item.profile;
    return group ? groupDefault(model, *group) : kDefaultProfile;
}

QString effective(const Model &model, const QString &taskId, const QString &itemId)
{
    const Item *item = model.item(taskId, itemId);
    if (!item)
        return kDefaultProfile;
    return effective(model, *item, groupOfTask(model, taskId));
}

QString effectiveFor(const Model &model, const QString &taskId, const QString &itemProfile)
{
    Item item;
    item.profile = itemProfile;
    return effective(model, item, groupOfTask(model, taskId));
}

bool remove(Model &model, const QString &id)
{
    if (id == kDefaultProfile || !model.profile(id))
        return false;
    std::vector<Profile> profiles = model.profiles();
    profiles.erase(std::remove_if(profiles.begin(), profiles.end(),
                                  [&id](const Profile &profile) { return profile.id == id; }),
                   profiles.end());
    model.setProfiles(profiles);
    return true;
}

} // namespace mirante::profiles
