#include <mirante/widgets/LinkStyle.h>

#include <QAbstractButton>

namespace mirante::widgets {

void styleAsLink(QAbstractButton *button)
{
    button->setCursor(Qt::PointingHandCursor);
    // A click leaves no underline behind, so the underline shows the keyboard's
    // place only.
    button->setFocusPolicy(Qt::TabFocus);
    button->setStyleSheet(QStringLiteral(
        "QPushButton, QToolButton { border: none; background: transparent; color: palette(link); padding: 1px 2px; }"
        "QPushButton:hover, QToolButton:hover, QPushButton:focus, QToolButton:focus { text-decoration: underline; }"
        "QPushButton:disabled, QToolButton:disabled { color: palette(mid); text-decoration: none; }"));
}

} // namespace mirante::widgets
