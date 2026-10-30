#include "ui/QuickAddDialog.h"

#include "core/Model.h"
#include "items/ItemTypeRegistry.h"
#include "items/QuickAddRun.h"

namespace mirante {

// Stub until T3 of the M7 plan.

struct QuickAddDialog::Private
{
    QString created;
};

QuickAddDialog::QuickAddDialog(Model &model, const ItemTypeRegistry &types, QuickAddRun &run,
                               const QString &profileId, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    Q_UNUSED(model)
    Q_UNUSED(types)
    Q_UNUSED(run)
    Q_UNUSED(profileId)
    setWindowTitle(tr("Quick Add"));
}

QuickAddDialog::~QuickAddDialog() = default;

void QuickAddDialog::setText(const QString &text) { Q_UNUSED(text) }

QString QuickAddDialog::createdTaskId() const { return d->created; }

} // namespace mirante
