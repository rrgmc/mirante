#include "ui/SettingsDialog.h"

#include "core/DeviceSettings.h"
#include "items/ItemTypeRegistry.h"
#include "ui/FieldForm.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSysInfo>
#include <QVBoxLayout>

namespace mirante {

struct SettingsDialog::Private
{
    DeviceSettings *settings = nullptr;
    QVBoxLayout *sections = nullptr;
    QCheckBox *sidebarHoverTint = nullptr;
    QLabel *hotkeyError = nullptr;
    QCheckBox *closeToTray = nullptr;
    bool modulesAdded = false;
};

SettingsDialog::SettingsDialog(DeviceSettings &settings, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->settings = &settings;
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowTitle(tr("Settings"));

    auto *note = new QLabel(tr("These settings apply to this device only. They are not saved in the workspace."), this);
    note->setObjectName(QStringLiteral("note"));
    note->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(note);
    d->sections = new QVBoxLayout;
    d->sections->setSpacing(12);
    layout->addLayout(d->sections);
    layout->addStretch(1);
    layout->addWidget(buttons);

    // Sidebar.
    QFormLayout *sidebar = addSection(tr("Sidebar"));
    d->sidebarHoverTint = new QCheckBox(tr("Highlight the sidebar row of the cell under the mouse"), this);
    d->sidebarHoverTint->setObjectName(QStringLiteral("sidebarHoverTint"));
    d->sidebarHoverTint->setToolTip(
        tr("While the mouse is over a dashboard cell, the sidebar row of the task or item it shows gets a soft tint."));
    d->sidebarHoverTint->setChecked(settings.sidebarHoverTint());
    connect(d->sidebarHoverTint, &QCheckBox::toggled, this, [this](bool on) {
        d->settings->setSidebarHoverTint(on);
        emit changed();
    });
    sidebar->addRow(d->sidebarHoverTint);

    // Status. The device setting is in seconds; the window shows whole
    // minutes.
    QFormLayout *status = addSection(tr("Status"));
    auto *pollInterval = new QSpinBox(this);
    pollInterval->setObjectName(QStringLiteral("pollInterval"));
    pollInterval->setRange(1, 60);
    pollInterval->setSuffix(tr(" min"));
    pollInterval->setToolTip(
        tr("How often Mirante asks servers, such as GitLab and Jira, for the status of items."));
    pollInterval->setValue(qBound(1, (settings.pollInterval() + 30) / 60, 60));
    connect(pollInterval, &QSpinBox::valueChanged, this, [this](int minutes) {
        d->settings->setPollInterval(minutes * 60);
        emit changed();
    });
    status->addRow(tr("Check servers every"), pollInterval);

    // Window.
    QFormLayout *window = addSection(tr("Window"));
    auto *hotkey = new QKeySequenceEdit(this);
    hotkey->setObjectName(QStringLiteral("globalHotkey"));
    hotkey->setMaximumSequenceLength(1);
    hotkey->setToolTip(tr("Shows or hides Mirante from any application."));
    hotkey->setKeySequence(QKeySequence::fromString(settings.globalHotkey(), QKeySequence::PortableText));
    connect(hotkey, &QKeySequenceEdit::keySequenceChanged, this, [this](const QKeySequence &sequence) {
        d->settings->setGlobalHotkey(sequence.toString(QKeySequence::PortableText));
        emit changed();
    });
    auto *clear = new QPushButton(tr("Clear"), this);
    clear->setObjectName(QStringLiteral("clearGlobalHotkey"));
    connect(clear, &QPushButton::clicked, hotkey, &QKeySequenceEdit::clear);
    auto *hotkeyRow = new QHBoxLayout;
    hotkeyRow->addWidget(hotkey, 1);
    hotkeyRow->addWidget(clear);
    window->addRow(tr("Global hotkey"), hotkeyRow);
    d->hotkeyError = new QLabel(this);
    d->hotkeyError->setObjectName(QStringLiteral("globalHotkeyError"));
    d->hotkeyError->setWordWrap(true);
    d->hotkeyError->hide();
    window->addRow(d->hotkeyError);
    d->closeToTray = new QCheckBox(tr("Closing the window keeps Mirante running in the tray"), this);
    d->closeToTray->setObjectName(QStringLiteral("closeToTray"));
    d->closeToTray->setChecked(settings.closeToTray());
    connect(d->closeToTray, &QCheckBox::toggled, this, [this](bool on) {
        d->settings->setCloseToTray(on);
        emit changed();
    });
    window->addRow(d->closeToTray);

    // Terminal. See docs/integrations.md#terminal-launch.
    QFormLayout *terminal = addSection(tr("Terminal"));
    auto *terminalCommand = new QLineEdit(settings.terminalCommand(), this);
    terminalCommand->setObjectName(QStringLiteral("terminalCommand"));
    terminalCommand->setPlaceholderText(tr("The system's terminal, or such as: wezterm start --cwd {dir} -- {command}"));
    terminalCommand->setToolTip(
        tr("The command that opens a terminal. {dir} is the folder to start in. {command} is the program to run, "
           "with its arguments. Empty uses the system's terminal."));
    connect(terminalCommand, &QLineEdit::textChanged, this, [this](const QString &text) {
        d->settings->setTerminalCommand(text);
        emit changed();
    });
    terminal->addRow(tr("Terminal command"), terminalCommand);
    // The field is empty while the device keeps the host name.
    const QString hostName = QSysInfo::machineHostName();
    auto *deviceName = new QLineEdit(settings.deviceName() == hostName ? QString() : settings.deviceName(), this);
    deviceName->setObjectName(QStringLiteral("deviceName"));
    deviceName->setPlaceholderText(hostName);
    deviceName->setToolTip(tr("Shown on items that only work on this device, such as a local Claude session."));
    connect(deviceName, &QLineEdit::textChanged, this, [this](const QString &text) {
        d->settings->setDeviceName(text);
        emit changed();
    });
    terminal->addRow(tr("Device name"), deviceName);

    resize(480, sizeHint().height());
}

SettingsDialog::~SettingsDialog() = default;

QCheckBox *SettingsDialog::sidebarHoverTint() const { return d->sidebarHoverTint; }

void SettingsDialog::setHotkeyError(const QString &error)
{
    d->hotkeyError->setText(error);
    d->hotkeyError->setVisible(!error.isEmpty());
}

void SettingsDialog::setTrayAvailable(bool available)
{
    const QSignalBlocker blocker(d->closeToTray);
    d->closeToTray->setChecked(available && d->settings->closeToTray());
    d->closeToTray->setEnabled(available);
}

// Each module's fields show the stored value, else the field's default. Only
// a value the user changes is stored, so an untouched default follows the
// module's. Empty text removes the value, so the default applies again.
void SettingsDialog::setModules(const ItemTypeRegistry &types)
{
    const QStringList moduleIds = types.deviceSettingModules();
    if (d->modulesAdded || moduleIds.isEmpty())
        return;
    d->modulesAdded = true;
    QFormLayout *modules = addSection(tr("Modules"));
    for (const QString &moduleId : moduleIds) {
        const std::vector<Field> fields = types.deviceSettings(moduleId);
        QJsonObject shown;
        for (const Field &field : fields) {
            const QJsonValue stored = d->settings->moduleSetting(moduleId, field.key);
            const QJsonValue value = stored.isUndefined() ? field.defaultValue : stored;
            if (!value.isUndefined() && !value.isNull())
                shown.insert(field.key, value);
        }
        auto *box = new QGroupBox(types.moduleName(moduleId), this);
        box->setObjectName(QStringLiteral("deviceSettings.") + moduleId);
        auto *form = new FieldForm(fields, EditorContext(), box);
        form->setObjectName(QStringLiteral("deviceSettingsForm.") + moduleId);
        form->setConfig(shown);
        auto *boxLayout = new QVBoxLayout(box);
        boxLayout->addWidget(form);
        modules->addRow(box);
        connect(form, &FieldForm::changed, this, [this, form, moduleId, fields, shown]() mutable {
            const QJsonObject values = form->config();
            for (const Field &field : fields) {
                const QJsonValue value = values.value(field.key);
                if (value == shown.value(field.key))
                    continue;
                shown.insert(field.key, value);
                const bool empty = value.isUndefined() || (value.isString() && value.toString().trimmed().isEmpty());
                d->settings->setModuleSetting(moduleId, field.key, empty ? QJsonValue() : value);
            }
            emit changed();
        });
    }
    resize(width(), sizeHint().height());
}

QFormLayout *SettingsDialog::addSection(const QString &title)
{
    auto *heading = new QLabel(title, this);
    QFont font = heading->font();
    font.setBold(true);
    heading->setFont(font);

    auto *form = new QFormLayout;
    form->setContentsMargins(12, 0, 0, 0);

    auto *section = new QVBoxLayout;
    section->setSpacing(4);
    section->addWidget(heading);
    section->addLayout(form);
    d->sections->addLayout(section);
    return form;
}

} // namespace mirante
