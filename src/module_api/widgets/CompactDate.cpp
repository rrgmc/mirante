#include <mirante/widgets/CompactDate.h>

#include <QDate>

namespace mirante::widgets {

QString compactDate(const QDateTime &when)
{
    if (!when.isValid())
        return {};
    const QDateTime local = when.toLocalTime();
    return local.toString(local.date().year() == QDate::currentDate().year()
                                  ? QStringLiteral("MM-dd HH:mm")
                                  : QStringLiteral("yyyy-MM-dd"));
}

} // namespace mirante::widgets
