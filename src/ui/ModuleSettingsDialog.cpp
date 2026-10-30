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

ModuleSettingsDialog::ModuleSettingsDialog(const QString &profileName, const QString &moduleName,
                                           const SettingsScreen &screen, const QJsonObject &values, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    setWindowTitle(tr("%1: %2 %3").arg(profileName, moduleName, screen.name()));
    resize(480, 400);

    // Each profile has its own values; items use those of their profile.
    auto *note = new QLabel(tr("These settings belong to the profile \"%1\". Items that use it read them. They are "
                               "saved in the workspace and shared with it; tokens stay on this device.")
                                .arg(profileName),
                            this);
    note->setObjectName(QStringLiteral("note"));
    note->setWordWrap(true);

    d->editor = screen.createEditor(this);
    if (!d->editor)
        d->editor = new FieldForm(screen.fields(), EditorContext(), this);
    d->editor->setObjectName(QStringLiteral("editor"));
    // A field without a stored value shows its default as real text, such as
    // GitLab's https://gitlab.com, since that is the value the module uses.
    QJsonObject shown = values;
    for (const Field &field : screen.fields()) {
        if (!shown.contains(field.key) && !field.defaultValue.isNull() && !field.defaultValue.isUndefined())
            shown.insert(field.key, field.defaultValue);
    }
    d->editor->setConfig(shown);

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

QJsonObject ModuleSettingsDialog::secrets() const
{
    const auto *form = qobject_cast<const FieldForm *>(d->editor);
    return form ? form->secrets() : QJsonObject();
}

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
