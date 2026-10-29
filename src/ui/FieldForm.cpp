#include "ui/FieldForm.h"

// Stubs written by T0 of the M3 plan. T5 implements them.

namespace mirante {

struct FieldForm::Private
{
    QJsonObject config;
};

FieldForm::FieldForm(std::vector<Field>, EditorContext, QWidget *parent)
    : ItemEditor(parent)
    , d(std::make_unique<Private>())
{
}

FieldForm::~FieldForm() = default;

void FieldForm::setConfig(const QJsonObject &config) { d->config = config; }
QJsonObject FieldForm::config() const { return d->config; }
QStringList FieldForm::problems() const { return {}; }
QWidget *FieldForm::editorFor(const QString &) const { return nullptr; }

} // namespace mirante
