#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

#include <memory>

namespace mirante {

class ItemTypeRegistry;
class Model;
class QuickAddRun;

// The quick add dialog: paste text, check the proposed title, project and
// items, and create the task, "New Task from Link". Opened for a task, it adds
// the items to that task instead, "Add Items from Link". See
// docs/ui.md#quick-add and decisions 0078 and 0083. Implemented in
// QuickAddDialog.cpp.
class QuickAddDialog : public QDialog
{
    Q_OBJECT

public:
    // run does the work; the dialog does not own it. profileId is the profile
    // it starts with: the selected group's, or empty for the workspace
    // default. Everything passed must outlive the dialog.
    QuickAddDialog(Model &model, const ItemTypeRegistry &types, QuickAddRun &run, const QString &profileId,
                   QWidget *parent = nullptr);
    ~QuickAddDialog() override;

    // The dialog that adds items to an existing task. It has no Title and no
    // Project: the project is the task's, and the profile starts as its
    // group's. It sets the run's task. The caller owns the dialog, through
    // parent or otherwise.
    static QuickAddDialog *forTask(Model &model, const ItemTypeRegistry &types, QuickAddRun &run,
                                   const QString &taskId, QWidget *parent = nullptr);

    // Fills the text box, and starts reading it, as a paste does.
    void setText(const QString &text);

    // The task Create made, or empty.
    QString createdTaskId() const;

    // The items Add Items added, in order, or none.
    QStringList addedItemIds() const;

signals:
    // Create Task made a task. The main window opens it in the preview slot.
    void taskCreated(const QString &taskId);
    // Add Items added items to the task. The main window reveals the task.
    void itemsAdded(const QString &taskId, const QStringList &itemIds);

private:
    QuickAddDialog(Model &model, const ItemTypeRegistry &types, QuickAddRun &run, const QString &profileId,
                   const QString &taskId, QWidget *parent);

    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
