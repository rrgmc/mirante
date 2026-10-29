#pragma once

#include "core/Entities.h"

#include <mirante/ItemEditor.h>
#include <mirante/ModuleSettings.h>

#include <QDialog>
#include <QJsonObject>

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace mirante {

// Edits the values of one module settings screen, from the Modules menu. A
// profile list at the top chooses whose values are shown; Default is chosen
// first. Below it is the screen's own editor, or else a FieldForm built from
// its fields. Values edited for several profiles are all kept until OK or
// Cancel. Keys the editor does not show are kept. OK, and choosing another
// profile, are refused while the editor reports problems. See
// docs/ui.md#modules and decisions 0044 and 0050 in docs/decisions/.
// Implemented in ModuleSettingsDialog.cpp.
class ModuleSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // moduleName names the module in the title, such as "GitLab". profiles are
    // offered in their order, from profiles::all(). stored reads a profile's
    // values of this screen.
    ModuleSettingsDialog(const QString &moduleName, const SettingsScreen &screen,
                         const std::vector<Profile> &profiles,
                         std::function<QJsonObject(const QString &profileId)> stored, QWidget *parent = nullptr);
    ~ModuleSettingsDialog() override;

    // The profile whose values are shown.
    QString profileId() const;
    // Shows the values of another profile, as choosing it in the list does.
    // Returns false, and stays, while the editor reports problems.
    bool setProfile(const QString &profileId);

    // The edited values, by profile id: only the profiles whose values the
    // user changed. Keys the editor does not show are as given.
    std::map<QString, QJsonObject> values() const;

    // The secrets typed into the form's Secret fields, such as a token, by
    // profile id, then by field key. They are not part of values(); the
    // caller keeps them in the keychain. See decision 0054.
    std::map<QString, QJsonObject> secrets() const;

    ItemEditor *editor() const;   // for tests

    void accept() override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
