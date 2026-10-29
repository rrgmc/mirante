#include "ui/GroupDialog.h"

// Stubs written by T0 of the M3 plan. T9 implements them.

namespace mirante {

struct GroupDialog::Private
{
    Group group;
};

GroupDialog::GroupDialog(const Group &group, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->group = group;
}

GroupDialog::~GroupDialog() = default;

Group GroupDialog::value() const { return d->group; }
void GroupDialog::setName(const QString &name) { d->group.name = name; }
void GroupDialog::accept() { QDialog::accept(); }

} // namespace mirante
