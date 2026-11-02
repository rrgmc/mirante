#pragma once

#include <QJsonValue>
#include <QStringList>
#include <QWidget>

namespace mirante::widgets {

// A module's own editor of one field in a FieldForm, which the rest of the form
// does not notice. See FieldForm::setFieldEditor() and decision 0122.
class FieldEditor : public QWidget
{
    Q_OBJECT

public:
    using QWidget::QWidget;

    // Shows the stored value. Undefined when none is stored.
    virtual void setValue(const QJsonValue &value) = 0;
    // The value to store, or Undefined to remove its key.
    virtual QJsonValue value() const = 0;
    // What is wrong with the value, beyond the checks of the field's type.
    virtual QStringList problems() const { return {}; }

signals:
    // The user changed the value.
    void changed();
};

} // namespace mirante::widgets
