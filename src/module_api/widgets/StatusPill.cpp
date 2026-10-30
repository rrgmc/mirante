#include <mirante/widgets/StatusPill.h>

#include <QPainter>
#include <QPalette>

namespace mirante::widgets {

namespace {

constexpr int horizontalPadding = 8;
constexpr int verticalPadding = 2;

} // namespace

StatusColours statusColours(StatusRole role, const QPalette &palette)
{
    // A scroll area's content can have a transparent window colour, which says
    // nothing about the theme. The text colour then tells.
    const QColor window = palette.color(QPalette::Window);
    const bool dark = window.alpha() == 0 ? palette.color(QPalette::WindowText).lightness() >= 128
                                          : window.lightness() < 128;
    switch (role) {
    case StatusRole::Good:
        return dark ? StatusColours{QColor(0x1f, 0x4d, 0x2c), QColor(0xb7, 0xeb, 0xc3)}
                    : StatusColours{QColor(0xd3, 0xf0, 0xd8), QColor(0x11, 0x60, 0x2a)};
    case StatusRole::Bad:
        return dark ? StatusColours{QColor(0x5c, 0x23, 0x23), QColor(0xfb, 0xcf, 0xca)}
                    : StatusColours{QColor(0xfa, 0xdc, 0xd9), QColor(0x9b, 0x1c, 0x1c)};
    case StatusRole::Waiting:
        return dark ? StatusColours{QColor(0x57, 0x41, 0x16), QColor(0xfb, 0xe3, 0xa6)}
                    : StatusColours{QColor(0xfb, 0xec, 0xcb), QColor(0x7a, 0x4f, 0x00)};
    case StatusRole::Neutral:
        break;
    }
    return dark ? StatusColours{QColor(0x3d, 0x41, 0x47), QColor(0xdd, 0xe0, 0xe4)}
                : StatusColours{QColor(0xe4, 0xe6, 0xe9), QColor(0x3f, 0x44, 0x4b)};
}

StatusPill::StatusPill(const QString &text, StatusRole role, QWidget *parent)
    : QLabel(parent)
    , m_role(role)
{
    // The text often comes from a server.
    setTextFormat(Qt::PlainText);
    setContentsMargins(horizontalPadding, verticalPadding, horizontalPadding, verticalPadding);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setText(text);
}

void StatusPill::setStatus(const QString &text, StatusRole role)
{
    setText(text);
    m_role = role;
    update();
}

// Painted here and not by a style sheet, so that the colours follow a change of
// palette without any further work.
void StatusPill::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    const StatusColours colours = statusColours(m_role, palette());
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(colours.fill);
    const QRectF area = rect();
    painter.drawRoundedRect(area, area.height() / 2, area.height() / 2);

    // A row can give a pill less than it asked for in a very narrow cell.
    const QRect textArea = contentsRect();
    painter.setPen(colours.text);
    painter.drawText(textArea, Qt::AlignCenter,
                     fontMetrics().elidedText(text(), Qt::ElideRight, textArea.width()));
}

} // namespace mirante::widgets
