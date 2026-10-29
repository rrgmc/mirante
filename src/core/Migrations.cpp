#include "core/Migrations.h"

// Stubs written by T0 of the M3 plan. T2 implements them.

namespace mirante::migrations {

bool migrate(FileKind kind, QJsonObject &json, QString *error)
{
    return migrate(kind, json, {}, error);
}

bool migrate(FileKind, QJsonObject &, const std::vector<Step> &, QString *) { return true; }

} // namespace mirante::migrations
