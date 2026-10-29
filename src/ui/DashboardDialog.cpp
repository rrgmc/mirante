#include "ui/DashboardDialog.h"

#include "layout/Presets.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace mirante {

struct DashboardDialog::Private
{
    Dashboard dashboard;
    QLineEdit *name = nullptr;
    QComboBox *preset = nullptr;    // only when creating
    QLabel *message = nullptr;      // why OK was refused
};

DashboardDialog::DashboardDialog(const Dashboard &dashboard, bool creating, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->dashboard = dashboard;

    setWindowTitle(creating ? tr("New dashboard") : tr("Rename dashboard"));

    d->name = new QLineEdit(dashboard.name, this);
    d->name->setObjectName(QStringLiteral("name"));
    d->message = new QLabel(this);
    d->message->setObjectName(QStringLiteral("message"));
    d->message->setWordWrap(true);
    d->message->hide();

    auto *form = new QFormLayout;
    form->addRow(tr("Name:"), d->name);
    if (creating) {
        d->preset = new QComboBox(this);
        d->preset->setObjectName(QStringLiteral("preset"));
        d->preset->addItems(presets::names());
        d->preset->setCurrentIndex(0);
        form->addRow(tr("Layout:"), d->preset);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &DashboardDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(d->message);
    layout->addWidget(buttons);

    d->name->setFocus();
}

DashboardDialog::~DashboardDialog() = default;

Dashboard DashboardDialog::value() const
{
    Dashboard dashboard = d->dashboard;
    dashboard.name = d->name->text().trimmed();
    return dashboard;
}

int DashboardDialog::presetIndex() const { return d->preset ? d->preset->currentIndex() : -1; }

void DashboardDialog::setName(const QString &name) { d->name->setText(name); }

void DashboardDialog::setPresetIndex(int index)
{
    if (d->preset)
        d->preset->setCurrentIndex(index);
}

void DashboardDialog::accept()
{
    if (d->name->text().trimmed().isEmpty()) {
        d->message->setText(tr("Enter a name."));
        d->message->show();
        d->name->setFocus();
        return;
    }
    QDialog::accept();
}

} // namespace mirante
