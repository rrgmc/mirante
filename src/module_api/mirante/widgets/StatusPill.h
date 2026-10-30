#pragma once

#include <QColor>
#include <QLabel>

class QPalette;

namespace mirante::widgets {

// What a status means, so that modules name a role and never a colour.
enum class StatusRole { Good, Bad, Waiting, Neutral };

struct StatusColours
{
    QColor fill;
    QColor text;
};

// The colours of a role, with one pair for light palettes and one for dark
// ones. The only place that names the colours of a status.
StatusColours statusColours(StatusRole role, const QPalette &palette);

// A small rounded label, such as "Failed", in the colours of its role.
class StatusPill : public QLabel
{
public:
    StatusPill(const QString &text, StatusRole role, QWidget *parent = nullptr);

    void setStatus(const QString &text, StatusRole role);
    StatusRole role() const { return m_role; }

private:
    StatusRole m_role;
};

} // namespace mirante::widgets
