#include "ui/DashboardDialog.h"

// Stubs written by T0 of the M3 plan. T9 implements them.

namespace mirante {

struct DashboardDialog::Private
{
    Dashboard dashboard;
    int preset = -1;
};

DashboardDialog::DashboardDialog(const Dashboard &dashboard, bool creating, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->dashboard = dashboard;
    d->preset = creating ? 0 : -1;
}

DashboardDialog::~DashboardDialog() = default;

Dashboard DashboardDialog::value() const { return d->dashboard; }
int DashboardDialog::presetIndex() const { return d->preset; }
void DashboardDialog::setName(const QString &name) { d->dashboard.name = name; }
void DashboardDialog::setPresetIndex(int index) { d->preset = index; }
void DashboardDialog::accept() { QDialog::accept(); }

} // namespace mirante
