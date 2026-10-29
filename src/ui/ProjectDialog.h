#pragma once

#include "core/Entities.h"

#include <QDialog>
#include <QMap>

#include <memory>

namespace mirante {

class DeviceSettings;
class Model;

// Creates or edits a project: name, group, priority and folders. Each folder
// has a name, a remote URL, the main flag, and its local path on this device,
// shown as "Folder not set on this device" when empty, with "Choose…".
// Exactly one folder is main when there are any. Module settings and
// connection overrides arrive in M5. OK is refused while the name is empty.
// See the M3 plan, part 5. Implemented in ProjectDialog.cpp.
class ProjectDialog : public QDialog
{
    Q_OBJECT

public:
    // settings gives the folders' current local paths. The dialog does not
    // write them; the caller saves folderPaths() after OK.
    ProjectDialog(const Project &project, const Model &model, const DeviceSettings &settings,
                  QWidget *parent = nullptr);
    ~ProjectDialog() override;

    // The project with the edited fields; every other field as given. New
    // folders get new ids.
    Project value() const;
    // The local path of each folder of value(), by folder id; empty for none.
    QMap<QString, QString> folderPaths() const;

    // For tests.
    void setName(const QString &name);
    void setGroupId(const QString &groupId);
    void setPriority(Priority priority);
    void setFolders(const std::vector<Folder> &folders, const QMap<QString, QString> &paths);

    void accept() override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
