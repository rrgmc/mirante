#include "items/FieldValidation.h"

#include <QCoreApplication>
#include <QUrl>

#include <cmath>
#include <limits>

namespace mirante {

namespace {

bool holdsText(Field::Type type)
{
    switch (type) {
    case Field::Type::Text:
    case Field::Type::MultiLineText:
    case Field::Type::Url:
    case Field::Type::Choice:
    case Field::Type::Folder:
    case Field::Type::Image:
        return true;
    case Field::Type::Integer:
    case Field::Type::Bool:
        return false;
    }
    return false;
}

// A missing or null value, or an empty string for a field that holds text.
bool isEmpty(const Field &field, const QJsonValue &value)
{
    if (value.isUndefined() || value.isNull())
        return true;
    return holdsText(field.type) && value.isString() && value.toString().trimmed().isEmpty();
}

bool isInteger(const QJsonValue &value)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    return std::isfinite(number) && std::floor(number) == number
        && std::abs(number) <= static_cast<double>(std::numeric_limits<qint64>::max());
}

bool isWebUrl(const QString &text)
{
    const QUrl url(text.trimmed(), QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    return url.isValid() && (scheme == QLatin1String("http") || scheme == QLatin1String("https"))
        && !url.host().isEmpty();
}

// The message for a value that is not empty, or an empty string if it is valid.
QString check(const Field &field, const QJsonValue &value)
{
    const char *message = nullptr;
    switch (field.type) {
    case Field::Type::Text:
    case Field::Type::MultiLineText:
    case Field::Type::Folder:
    case Field::Type::Image:
        if (!value.isString())
            message = QT_TRANSLATE_NOOP("FieldValidation", "%1 must be text");
        break;
    case Field::Type::Url:
        if (!value.isString() || !isWebUrl(value.toString()))
            message = QT_TRANSLATE_NOOP("FieldValidation", "%1 must be an address starting with http:// or https://");
        break;
    case Field::Type::Integer:
        if (!isInteger(value))
            message = QT_TRANSLATE_NOOP("FieldValidation", "%1 must be a whole number");
        break;
    case Field::Type::Bool:
        if (!value.isBool())
            message = QT_TRANSLATE_NOOP("FieldValidation", "%1 must be on or off");
        break;
    case Field::Type::Choice:
        if (!value.isString() || !field.choices.contains(value.toString()))
            message = QT_TRANSLATE_NOOP("FieldValidation", "%1 must be one of the choices offered");
        break;
    }
    return message ? QCoreApplication::translate("FieldValidation", message).arg(field.label) : QString();
}

} // namespace

std::vector<FieldProblem> validate(const std::vector<Field> &fields, const QJsonObject &config)
{
    std::vector<FieldProblem> problems;
    for (const Field &field : fields) {
        const QJsonValue value = config.value(field.key);
        QString message;
        if (isEmpty(field, value)) {
            if (field.required)
                message = QCoreApplication::translate("FieldValidation", "%1 is required").arg(field.label);
        } else {
            message = check(field, value);
        }
        if (!message.isEmpty())
            problems.push_back({field.key, message});
    }
    return problems;
}

QJsonObject defaultConfig(const std::vector<Field> &fields)
{
    QJsonObject config;
    for (const Field &field : fields) {
        if (!field.defaultValue.isNull() && !field.defaultValue.isUndefined())
            config.insert(field.key, field.defaultValue);
    }
    return config;
}

} // namespace mirante
