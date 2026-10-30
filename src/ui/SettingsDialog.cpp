#include "ui/SettingsDialog.h"

#include "core/DeviceSettings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace mirante {

struct SettingsDialog::Private
{
    DeviceSettings *settings = nullptr;
    QVBoxLayout *sections = nullptr;
    QCheckBox *sidebarHoverTint = nullptr;
    QLabel *hotkeyError = nullptr;
    QCheckBox *closeToTray = nullptr;
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
