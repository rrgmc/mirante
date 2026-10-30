#include <mirante/widgets/CompactDate.h>

#include <QDate>
#include <QLocale>

namespace mirante::widgets {

QString compactDate(const QDateTime &when)
{
    if (!when.isValid())
        return {};
    // Numeric, so the same in every language, but formatted by the chosen
    // language's locale, as every date shown.
    const QDateTime local = when.toLocalTime();
    return QLocale().toString(local, local.date().year() == QDate::currentDate().year()
                                         ? QStringLiteral("MM-dd HH:mm")
                                         : QStringLiteral("yyyy-MM-dd"));
}

} // namespace mirante::widgets
