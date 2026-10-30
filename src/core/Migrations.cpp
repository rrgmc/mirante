#include "core/Migrations.h"

#include <QCoreApplication>
#include <QJsonValue>

#include <cmath>

namespace mirante::migrations {

namespace {

const QString kSchemaVersion = QStringLiteral("schemaVersion");

// The real steps. Version 1 is the only version, so there are none yet.
const std::vector<Step> &realSteps()
{
    static const std::vector<Step> steps;
    return steps;
}

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

} // namespace

bool migrate(FileKind kind, QJsonObject &json, QString *error)
{
    return migrate(kind, json, realSteps(), error);
}

bool migrate(FileKind kind, QJsonObject &json, const std::vector<Step> &steps, QString *error)
{
    const int current = int(steps.size()) + 1;
    const QJsonValue value = json.value(kSchemaVersion);
    if (value.isUndefined())
        return fail(error, QCoreApplication::translate("Migrations", "missing schemaVersion"));
    const double number = value.toDouble(-1);
    if (!value.isDouble() || number < 1 || number != std::floor(number))
        return fail(error, QCoreApplication::translate("Migrations", "invalid schemaVersion"));
    if (number > current)
        return fail(error, QCoreApplication::translate("Migrations", "written by a newer version of Mirante"));

    for (int version = int(number); version < current; ++version)
        steps[std::size_t(version - 1)](kind, json);
    json.insert(kSchemaVersion, current);
    return true;
}

} // namespace mirante::migrations
