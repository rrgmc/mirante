#pragma once

#include <QDialog>
#include <QString>

#include <memory>

namespace mirante {

class ItemTypeRegistry;
class Model;
class QuickAddRun;

// The quick add dialog: paste text, check the proposed title, project and
// items, and create the task. See docs/ui.md#quick-add and decision 0078.
// Implemented in QuickAddDialog.cpp.
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

    // Fills the text box, and starts reading it, as a paste does.
    void setText(const QString &text);

    // The task Create made, or empty.
    QString createdTaskId() const;

signals:
    // Create made a task. The main window opens it in the preview slot.
    void taskCreated(const QString &taskId);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
