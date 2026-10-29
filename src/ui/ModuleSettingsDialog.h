#pragma once

#include <mirante/ItemEditor.h>
#include <mirante/ModuleSettings.h>

#include <QDialog>
#include <QJsonObject>

#include <memory>

namespace mirante {

// Edits the values of one module settings screen, from the Modules menu. It
// shows the screen's own editor, or else a FieldForm built from its fields.
// Keys the editor does not show are kept. OK is refused while the editor
// reports problems. See docs/ui.md#modules and
// docs/decisions/0044-module-settings-in-the-workspace.md. Implemented in
// ModuleSettingsDialog.cpp.
class ModuleSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // moduleName names the module in the title, such as "GitLab".
    ModuleSettingsDialog(const QString &moduleName, const SettingsScreen &screen, const QJsonObject &values,
                         QWidget *parent = nullptr);
    ~ModuleSettingsDialog() override;

    // The edited values; keys the editor does not show as given.
    QJsonObject values() const;

    ItemEditor *editor() const;   // for tests

    void accept() override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
