#include "ui/GroupDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace mirante {

struct GroupDialog::Private
{
    Group group;
    QLineEdit *name = nullptr;
    QLabel *message = nullptr;      // why OK was refused
};

GroupDialog::GroupDialog(const Group &group, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->group = group;

    setWindowTitle(group.name.isEmpty() ? tr("New group") : tr("Edit group"));

    d->name = new QLineEdit(group.name, this);
    d->name->setObjectName(QStringLiteral("name"));
    d->message = new QLabel(this);
    d->message->setObjectName(QStringLiteral("message"));
    d->message->setWordWrap(true);
    d->message->hide();

    auto *form = new QFormLayout;
    form->addRow(tr("Name:"), d->name);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &GroupDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(d->message);
    layout->addWidget(buttons);

    d->name->setFocus();
}

GroupDialog::~GroupDialog() = default;

Group GroupDialog::value() const
{
    Group group = d->group;
    group.name = d->name->text().trimmed();
    return group;
}

void GroupDialog::setName(const QString &name) { d->name->setText(name); }

void GroupDialog::accept()
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
