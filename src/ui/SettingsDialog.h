#pragma once

#include <QDialog>

#include <memory>

class QCheckBox;
class QFormLayout;

namespace mirante {

class DeviceSettings;

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

signals:
    // An option changed, and is already saved in the device settings.
    void changed();

private:
    // A heading, then an empty form for the section's options.
    QFormLayout *addSection(const QString &title);

    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
