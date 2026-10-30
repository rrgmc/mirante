#include "ui/SettingsDialog.h"

#include "core/DeviceSettings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace mirante {

struct SettingsDialog::Private
{
    DeviceSettings *settings = nullptr;
    QVBoxLayout *sections = nullptr;
    QCheckBox *sidebarHoverTint = nullptr;
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

    resize(480, sizeHint().height());
}

SettingsDialog::~SettingsDialog() = default;

QCheckBox *SettingsDialog::sidebarHoverTint() const { return d->sidebarHoverTint; }

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
