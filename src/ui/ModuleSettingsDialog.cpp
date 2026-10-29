#include "ui/ModuleSettingsDialog.h"

#include "ui/FieldForm.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

namespace mirante {

struct ModuleSettingsDialog::Private
{
    ItemEditor *editor = nullptr;
    QLabel *message = nullptr;   // why OK was refused
};

ModuleSettingsDialog::ModuleSettingsDialog(const QString &moduleName, const SettingsScreen &screen,
                                           const QJsonObject &values, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    setWindowTitle(tr("%1: %2").arg(moduleName, screen.name()));
    resize(480, 360);

    d->editor = screen.createEditor(this);
    if (!d->editor)
        d->editor = new FieldForm(screen.fields(), EditorContext(), this);
    d->editor->setObjectName(QStringLiteral("editor"));
    d->editor->setConfig(values);

    // The values are shared with everyone who opens the workspace.
    auto *note = new QLabel(tr("These settings are saved in the workspace and shared with it."), this);
    note->setObjectName(QStringLiteral("note"));
    note->setWordWrap(true);

    d->message = new QLabel(this);
    d->message->setObjectName(QStringLiteral("message"));
    d->message->setWordWrap(true);
    d->message->hide();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ModuleSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(note);
    layout->addWidget(d->editor, 1);
    layout->addWidget(d->message);
    layout->addWidget(buttons);
}

ModuleSettingsDialog::~ModuleSettingsDialog() = default;

QJsonObject ModuleSettingsDialog::values() const { return d->editor->config(); }

ItemEditor *ModuleSettingsDialog::editor() const { return d->editor; }

void ModuleSettingsDialog::accept()
{
    const QStringList problems = d->editor->problems();
    if (!problems.isEmpty()) {
        d->message->setText(problems.join(QLatin1Char('\n')));
        d->message->show();
        return;
    }
    QDialog::accept();
}

} // namespace mirante
