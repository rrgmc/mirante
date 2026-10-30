#pragma once

#include <mirante/ItemEditor.h>
#include <mirante/ModuleSettings.h>

#include <QDialog>
#include <QJsonObject>

#include <memory>

namespace mirante {

// Edits the values of one module settings screen in one profile, opened from
// Profiles > <profile> > Modules > <module> > <screen>. It shows the screen's
// own editor, or else a FieldForm built from its fields. Keys the editor does
// not show are kept. OK is refused while the editor reports problems. See
// docs/ui.md#modules and decisions 0044, 0050 and 0054 in docs/decisions/.
// Implemented in ModuleSettingsDialog.cpp.
class ModuleSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // The title names the profile, the module and the screen, such as
    // "Default: GitLab General Configuration". values are the screen's stored
    // values in that profile. A field the values lack shows its default.
    ModuleSettingsDialog(const QString &profileName, const QString &moduleName, const SettingsScreen &screen,
                         const QJsonObject &values, QWidget *parent = nullptr);
    ~ModuleSettingsDialog() override;

    // The edited values; keys the editor does not show as given.
    QJsonObject values() const;

    // The secrets typed into the form's Secret fields, such as a token, by
    // field key. They are never part of values(); the caller keeps them in
    // the keychain. See decision 0054.
    QJsonObject secrets() const;

    ItemEditor *editor() const;   // for tests

    void accept() override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
