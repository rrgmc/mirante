// Stub from task T0. Task T8 implements this file.
#include "core/Model.h"

namespace mirante {

struct Model::Private
{
};

Model::Model(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

Model::~Model() = default;

void Model::reset(std::vector<Group> /*groups*/, std::vector<Project> /*projects*/, std::vector<Task> /*tasks*/,
                  std::vector<RootDashboard> /*rootDashboards*/)
{
    emit modelReset();
}

const Group *Model::group(const QString & /*id*/) const { return nullptr; }
const Project *Model::project(const QString & /*id*/) const { return nullptr; }
const Task *Model::task(const QString & /*id*/) const { return nullptr; }
const Item *Model::item(const QString & /*taskId*/, const QString & /*itemId*/) const { return nullptr; }
const RootDashboard *Model::rootDashboard(const QString & /*id*/) const { return nullptr; }
const Dashboard *Model::taskDashboard(const QString & /*taskId*/, const QString & /*dashboardId*/) const
{
    return nullptr;
}
const LayoutNode *Model::layout(const DashboardRef & /*dashboard*/) const { return nullptr; }
const Task *Model::taskOfItem(const QString & /*itemId*/) const { return nullptr; }

std::vector<const Group *> Model::groups() const { return {}; }
std::vector<const Project *> Model::projectsOfGroup(const QString & /*groupId*/) const { return {}; }
std::vector<const Task *> Model::tasksOfProject(const QString & /*projectId*/) const { return {}; }
std::vector<const RootDashboard *> Model::rootDashboards() const { return {}; }

bool Model::refExists(const CellRef & /*ref*/) const { return false; }

void Model::setTaskDashboardLayout(const QString & /*taskId*/, const QString & /*dashboardId*/,
                                   const LayoutNode & /*tree*/)
{
}

void Model::setRootDashboardLayout(const QString & /*id*/, const LayoutNode & /*tree*/) {}

void Model::setLayout(const DashboardRef &dashboard, const LayoutNode &tree)
{
    if (dashboard.isRoot())
        setRootDashboardLayout(dashboard.dashboardId, tree);
    else
        setTaskDashboardLayout(dashboard.taskId, dashboard.dashboardId, tree);
}

void Model::addRootDashboard(const RootDashboard & /*dashboard*/) {}

} // namespace mirante
