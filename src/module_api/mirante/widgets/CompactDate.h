#pragma once

#include <QDateTime>
#include <QString>

namespace mirante::widgets {

// A date for a table, in local time: "10-30 14:32" in this year, "2025-10-30"
// otherwise. An invalid date gives an empty text. See decision 0071.
QString compactDate(const QDateTime &when);

} // namespace mirante::widgets
