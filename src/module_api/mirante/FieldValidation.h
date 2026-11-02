#pragma once

#include <mirante/Fields.h>

#include <QJsonObject>
#include <QString>

#include <vector>

// Checks an item configuration against its type's fields. See the M3 plan,
// part 5. Implemented in FieldValidation.cpp.

namespace mirante {

struct FieldProblem
{
    QString key;        // the field's key
    QString message;    // such as "Project path is required"
};

// One problem for each invalid field: a required field that is missing or
// empty, an integer that is not one, a URL without http or https, a choice not
// in the list, a value of the wrong JSON type. Keys not in fields are not
// checked.
std::vector<FieldProblem> validate(const std::vector<Field> &fields, const QJsonObject &config);

// The configuration of a new item: each field's default value, where it has one.
QJsonObject defaultConfig(const std::vector<Field> &fields);

} // namespace mirante
