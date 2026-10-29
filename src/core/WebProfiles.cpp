#include "core/WebProfiles.h"

#include "core/Model.h"

#include <QCoreApplication>

#include <algorithm>

namespace mirante::webprofiles {

namespace {

QString defaultName() { return QCoreApplication::translate("WebProfiles", "Default"); }

} // namespace

bool exists(const Model &model, const QString &id)
{
    return id == kDefaultWebProfile || model.webProfile(id);
}

std::vector<WebProfile> all(const Model &model)
{
    std::vector<WebProfile> listed = model.webProfiles();
    std::stable_sort(listed.begin(), listed.end(), [](const WebProfile &a, const WebProfile &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    WebProfile standard;
    standard.id = kDefaultWebProfile;
    standard.name = defaultName();
    listed.insert(listed.begin(), standard);
    return listed;
}

QString nameOf(const Model &model, const QString &id)
{
    if (id == kDefaultWebProfile)
        return defaultName();
    const WebProfile *profile = model.webProfile(id);
    return profile ? profile->name : QString();
}

QString groupDefault(const Model &model, const Group &group)
{
    return exists(model, group.webProfile) ? group.webProfile : kDefaultWebProfile;
}

QString effective(const Model &model, const Item &item, const Group *group)
{
    if (exists(model, item.webProfile))
        return item.webProfile;
    return group ? groupDefault(model, *group) : kDefaultWebProfile;
}

QString effective(const Model &model, const QString &taskId, const QString &itemId)
{
    const Item *item = model.item(taskId, itemId);
    if (!item)
        return kDefaultWebProfile;
    const Task *task = model.task(taskId);
    const Project *project = task ? model.project(task->projectId) : nullptr;
    const Group *group = project ? model.group(project->groupId) : nullptr;
    return effective(model, *item, group);
}

bool remove(Model &model, const QString &id)
{
    if (id == kDefaultWebProfile || !model.webProfile(id))
        return false;
    std::vector<WebProfile> profiles = model.webProfiles();
    profiles.erase(std::remove_if(profiles.begin(), profiles.end(),
                                  [&id](const WebProfile &profile) { return profile.id == id; }),
                   profiles.end());
    model.setWebProfiles(profiles);
    return true;
}

} // namespace mirante::webprofiles
