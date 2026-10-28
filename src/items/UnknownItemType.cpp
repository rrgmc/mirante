// Stub from task T0. Task T2 implements this file.
#include "items/UnknownItemType.h"

namespace mirante {

UnknownItemType::UnknownItemType(const QString &id)
    : m_id(id)
{
}

QString UnknownItemType::id() const { return m_id; }
QString UnknownItemType::name() const { return m_id; }
QIcon UnknownItemType::icon() const { return {}; }
QString UnknownItemType::moduleId() const { return m_id.section(QLatin1Char('.'), 0, 0); }

} // namespace mirante
