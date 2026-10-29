#include "ui/TaskDialog.h"

#include "core/Model.h"

// Stubs written by T0 of the M3 plan. T9 implements them.

namespace mirante {

struct TaskDialog::Private
{
    Task task;
};

TaskDialog::TaskDialog(const Task &task, const Model &, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->task = task;
}

TaskDialog::~TaskDialog() = default;

Task TaskDialog::value() const { return d->task; }
void TaskDialog::setTitle(const QString &title) { d->task.title = title; }
void TaskDialog::setProjectId(const QString &projectId) { d->task.projectId = projectId; }
void TaskDialog::setPriority(Priority priority) { d->task.priority = priority; }
void TaskDialog::setState(TaskState state) { d->task.state = state; }
void TaskDialog::accept() { QDialog::accept(); }

} // namespace mirante
