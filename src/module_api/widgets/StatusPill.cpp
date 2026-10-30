#include <mirante/widgets/StatusPill.h>

#include <QPalette>

namespace mirante::widgets {

StatusColours statusColours(StatusRole role, const QPalette &palette)
{
    Q_UNUSED(role)
    return {palette.color(QPalette::Mid), palette.color(QPalette::WindowText)};
}

StatusPill::StatusPill(const QString &text, StatusRole role, QWidget *parent)
    : QLabel(text, parent)
    , m_role(role)
{
}

void StatusPill::setStatus(const QString &text, StatusRole role)
{
    setText(text);
    m_role = role;
}

} // namespace mirante::widgets
