#pragma once

#include <QDialog>

#include <memory>

class QCheckBox;
class QFormLayout;

namespace mirante {

class DeviceSettings;
class ItemTypeRegistry;

// The application's settings window: Settings… in the application menu on
// macOS, Tools > Settings… elsewhere. It holds settings of this device, kept
// in DeviceSettings, not in the workspace. Each change is saved at once and
// reported with changed(), so the app applies it without a restart. The
// options are grouped in sections, each a heading over a form; a new option
// goes in a section with addSection(). See docs/ui.md#settings. Implemented in
// SettingsDialog.cpp.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(DeviceSettings &settings, QWidget *parent = nullptr);
    ~SettingsDialog() override;

    // "Highlight the sidebar row of the cell under the mouse".
    QCheckBox *sidebarHoverTint() const;
    // Shows why the global hotkey could not be registered, or nothing if empty.
    void setHotkeyError(const QString &error);
    // Without a tray, "keep running in the tray" is off and cannot be changed.
    // The stored choice stays, for when a tray appears.
    void setTrayAvailable(bool available);
    // Shows "Start Mirante when I sign in", checked as given. The system holds
    // this choice, not DeviceSettings, so the dialog only reports a change
    // with startAtSignInToggled(). Without a call the option is hidden, as
    // where the system has no way to do it.
    void setStartAtSignIn(bool on);
    QCheckBox *startAtSignIn() const;
    // Adds the Modules section: a group for each module with device settings,
    // built from their fields. Without any, there is no section. Call it once.
    // See decision 0079.
    void setModules(const ItemTypeRegistry &types);

signals:
    // An option changed, and is already saved in the device settings.
    void changed();
    // The user ticked or cleared "Start Mirante when I sign in".
    void startAtSignInToggled(bool on);

private:
    // A heading, then an empty form for the section's options.
    QFormLayout *addSection(const QString &title);

    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
