#include "ui/TaskDialog.h"

#include "core/Model.h"
#include "core/Sorting.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <algorithm>

namespace mirante {

namespace {

void selectData(QComboBox *combo, const QVariant &data)
{
    const int index = combo->findData(data);
    if (index >= 0)
        combo->setCurrentIndex(index);
}

} // namespace

struct TaskDialog::Private
{
    Task task;
    QLineEdit *title = nullptr;
    QComboBox *project = nullptr;
    QComboBox *priority = nullptr;
    QComboBox *state = nullptr;
    QLabel *message = nullptr;      // why OK was refused

    // Every group in manual order, each followed by its projects in display
    // order. Group rows are headings that cannot be chosen.
    void fillProjects(const Model &model)
    {
        auto *items = qobject_cast<QStandardItemModel *>(project->model());
        std::vector<const Group *> groups = model.groups();
        std::sort(groups.begin(), groups.end(), [](const Group *a, const Group *b) {
            return a->order != b->order ? a->order < b->order : a->id < b->id;
        });
        for (const Group *group : groups) {
            project->addItem(group->name);
            if (items)
                items->item(project->count() - 1)->setFlags(Qt::NoItemFlags);
            std::vector<const Project *> projects = model.projectsOfGroup(group->id);
            std::sort(projects.begin(), projects.end(),
                      [](const Project *a, const Project *b) { return projectBefore(*a, *b); });
            for (const Project *p : projects)
                project->addItem(QStringLiteral("    ") + p->name, p->id);
        }
    }
};

TaskDialog::TaskDialog(const Task &task, const Model &model, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->task = task;

    setWindowTitle(task.title.isEmpty() ? tr("New task") : tr("Edit task"));

    d->title = new QLineEdit(task.title, this);
    d->title->setObjectName(QStringLiteral("title"));

    d->project = new QComboBox(this);
    d->project->setObjectName(QStringLiteral("project"));
    d->fillProjects(model);
    if (d->project->findData(task.projectId) < 0 && !task.projectId.isEmpty())
        d->project->addItem(tr("(unknown project)"), task.projectId);
    selectData(d->project, task.projectId);
    if (d->project->currentIndex() >= 0 && !d->project->currentData().isValid()) {
        // A heading is selected: choose the first project instead.
        for (int i = 0; i < d->project->count(); ++i) {
            if (d->project->itemData(i).isValid()) {
                d->project->setCurrentIndex(i);
                break;
            }
        }
    }

    d->priority = new QComboBox(this);
    d->priority->setObjectName(QStringLiteral("priority"));
    d->priority->addItem(tr("Urgent"), int(Priority::Urgent));
    d->priority->addItem(tr("High"), int(Priority::High));
    d->priority->addItem(tr("Normal"), int(Priority::Normal));
    d->priority->addItem(tr("Low"), int(Priority::Low));
    selectData(d->priority, int(task.priority));

    d->state = new QComboBox(this);
    d->state->setObjectName(QStringLiteral("state"));
    d->state->addItem(tr("Active"), int(TaskState::Active));
    d->state->addItem(tr("Done"), int(TaskState::Done));
    d->state->addItem(tr("Archived"), int(TaskState::Archived));
    selectData(d->state, int(task.state));

    d->message = new QLabel(this);
    d->message->setObjectName(QStringLiteral("message"));
    d->message->setWordWrap(true);
    d->message->hide();

    auto *form = new QFormLayout;
    form->addRow(tr("Title:"), d->title);
    form->addRow(tr("Project:"), d->project);
    form->addRow(tr("Priority:"), d->priority);
    form->addRow(tr("State:"), d->state);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &TaskDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(d->message);
    layout->addWidget(buttons);

    d->title->setFocus();
}

TaskDialog::~TaskDialog() = default;

Task TaskDialog::value() const
{
    Task task = d->task;
    task.title = d->title->text().trimmed();
    const QVariant project = d->project->currentData();
    if (project.isValid())
        task.projectId = project.toString();
    task.priority = Priority(d->priority->currentData().toInt());
    task.state = TaskState(d->state->currentData().toInt());
    return task;
}

void TaskDialog::setTitle(const QString &title) { d->title->setText(title); }
void TaskDialog::setProjectId(const QString &projectId) { selectData(d->project, projectId); }
void TaskDialog::setPriority(Priority priority) { selectData(d->priority, int(priority)); }
void TaskDialog::setState(TaskState state) { selectData(d->state, int(state)); }

void TaskDialog::accept()
{
    if (d->title->text().trimmed().isEmpty()) {
        d->message->setText(tr("Enter a title."));
        d->message->show();
        d->title->setFocus();
        return;
    }
    QDialog::accept();
}

} // namespace mirante
