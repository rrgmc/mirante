#pragma once

#include <QDialog>

#include <memory>

class QFormLayout;

namespace mirante {

// Where a profile keeps its data on this device. The main window fills it
// from the ProfileManager and the workspace id.
struct ProfileStorage
{
    QString webData;        // the profile's folder: logins, cookies and site data
    QString cache;          // the web cache, inside the profile's folder
    QString tokenPrefix;    // the start of its keychain entries' names
};

// The settings window of one profile, opened from Profiles > <profile> >
// Settings…. Its Storage section shows where the profile's data is kept, each
// folder with an Open button. It is read-only for now; later sections may
// hold settings of the profile. See docs/ui.md#profiles-menu. Implemented in
// ProfileSettingsDialog.cpp.
class ProfileSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    ProfileSettingsDialog(const QString &profileName, const ProfileStorage &storage, QWidget *parent = nullptr);
    ~ProfileSettingsDialog() override;

private:
    QFormLayout *addSection(const QString &title);

    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
