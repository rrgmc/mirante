#pragma once

#include "core/Entities.h"

#include <QDialog>

#include <memory>

namespace mirante {

class Model;

// Creates or edits a task: title, project, priority and state. The project
// list holds every project, under its group. OK is refused while the title is
// empty. See the M3 plan, part 5. Implemented in TaskDialog.cpp.
class TaskDialog : public QDialog
{
    Q_OBJECT

public:
    TaskDialog(const Task &task, const Model &model, QWidget *parent = nullptr);
    ~TaskDialog() override;

    // The task with the edited fields; items, dashboards and every other field as given.
    Task value() const;

    // For tests.
    void setTitle(const QString &title);
    void setProjectId(const QString &projectId);
    void setPriority(Priority priority);
    void setState(TaskState state);

    void accept() override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
