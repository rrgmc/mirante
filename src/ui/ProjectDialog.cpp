#include "ui/ProjectDialog.h"

#include "core/DeviceSettings.h"
#include "core/Model.h"

// Stubs written by T0 of the M3 plan. T9 implements them.

namespace mirante {

struct ProjectDialog::Private
{
    Project project;
    QMap<QString, QString> paths;
};

ProjectDialog::ProjectDialog(const Project &project, const Model &, const DeviceSettings &, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->project = project;
}

ProjectDialog::~ProjectDialog() = default;

Project ProjectDialog::value() const { return d->project; }
QMap<QString, QString> ProjectDialog::folderPaths() const { return d->paths; }
void ProjectDialog::setName(const QString &name) { d->project.name = name; }
void ProjectDialog::setGroupId(const QString &groupId) { d->project.groupId = groupId; }
void ProjectDialog::setPriority(Priority priority) { d->project.priority = priority; }

void ProjectDialog::setFolders(const std::vector<Folder> &folders, const QMap<QString, QString> &paths)
{
    d->project.folders = folders;
    d->paths = paths;
}

void ProjectDialog::accept() { QDialog::accept(); }

} // namespace mirante
