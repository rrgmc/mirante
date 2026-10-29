#include "ui/AppStyle.h"

#include "ui/RootTabBar.h"

#include <QAbstractButton>
#include <QCursor>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>

namespace mirante {

namespace {

constexpr qreal radius = 5.0;

// The fills of flat controls: the text colour, mostly transparent, so they
// work on light and dark palettes alike.
QColor fill(const QPalette &palette, int alpha)
{
    QColor color = palette.color(QPalette::WindowText);
    color.setAlpha(alpha);
    return color;
}

constexpr int hoverAlpha = 18;
constexpr int strongAlpha = 36;

void fillRounded(QPainter *painter, const QRectF &rect, const QColor &color)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawRoundedRect(rect, radius, radius);
    painter->restore();
}

// Tool buttons in a toolbar keep the platform look.
bool isFlatToolButton(const QStyleOption *option, const QWidget *widget)
{
    if (!(option->state & QStyle::State_AutoRaise))
        return false;
    return !widget || !qobject_cast<const QToolBar *>(widget->parentWidget());
}

// The tab that holds a close button, or -1.
int tabOfButton(const QTabBar *bar, const QWidget *button)
{
    const auto side = QTabBar::ButtonPosition(
        bar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, bar));
    for (int i = 0; i < bar->count(); ++i) {
        if (bar->tabButton(i, side) == button)
            return i;
    }
    return -1;
}

} // namespace

AppStyle::AppStyle()
    : QProxyStyle()
{
}

void AppStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter,
                                  const QWidget *widget) const
{
    const auto *button = qstyleoption_cast<const QStyleOptionToolButton *>(option);
    if (control != CC_ToolButton || !button || !isFlatToolButton(option, widget)) {
        QProxyStyle::drawComplexControl(control, option, painter, widget);
        return;
    }

    const bool enabled = button->state & State_Enabled;
    const bool down = button->state & (State_Sunken | State_On);
    const bool hover = button->state & State_MouseOver;
    if (enabled && down)
        fillRounded(painter, button->rect, fill(button->palette, strongAlpha));
    else if (enabled && hover)
        fillRounded(painter, button->rect, fill(button->palette, hoverAlpha));

    // The label without the pressed state, which some styles draw as a shift
    // or as inverted text.
    QStyleOptionToolButton label = *button;
    label.rect = subControlRect(CC_ToolButton, button, SC_ToolButton, widget);
    label.state &= ~(State_Sunken | State_On | State_Raised);
    drawControl(CE_ToolButtonLabel, &label, painter, widget);

    // The menu arrow: beside the label for a split button, in the corner for
    // a button that opens its menu at once.
    QStyleOption arrow = *button;
    arrow.state &= ~(State_Sunken | State_On);
    if (button->subControls & SC_ToolButtonMenu) {
        arrow.rect = subControlRect(CC_ToolButton, button, SC_ToolButtonMenu, widget);
        drawPrimitive(PE_IndicatorArrowDown, &arrow, painter, widget);
    } else if (button->features & QStyleOptionToolButton::HasMenu) {
        const int size = 6;
        arrow.rect = QRect(button->rect.right() - size - 1, button->rect.bottom() - size - 1, size, size);
        drawPrimitive(PE_IndicatorArrowDown, &arrow, painter, widget);
    }
}

void AppStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                           const QWidget *widget) const
{
    const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option);
    if (element == CE_TabBarTabShape && tab) {
        const QRectF rect = QRectF(tab->rect).adjusted(1.5, 2.5, -1.5, -2.5);
        const bool enabled = tab->state & State_Enabled;
        if (tab->state & State_Selected)
            fillRounded(painter, rect, fill(tab->palette, strongAlpha));
        else if (enabled && (tab->state & State_MouseOver))
            fillRounded(painter, rect, fill(tab->palette, hoverAlpha));
        return;
    }
    if (element == CE_TabBarTabLabel && tab) {
        // The macOS style draws the current tab's text in white, for its
        // accent fill, except in document mode.
        QStyleOptionTab label = *tab;
        label.documentMode = true;
        QProxyStyle::drawControl(element, &label, painter, widget);
        return;
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}

void AppStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                             const QWidget *widget) const
{
    switch (element) {
    case PE_FrameTabBarBase: {
        // A hairline under the tabs instead of the platform's base.
        painter->save();
        painter->setPen(fill(option->palette, strongAlpha));
        const QRect &r = option->rect;
        painter->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
        painter->restore();
        return;
    }
    case PE_IndicatorTabClose: {
        if (!(option->state & State_Enabled))
            return;
        // Shown on the current tab, on the tab under the mouse, and while the
        // mouse is over the button itself. A bar whose selection is hidden,
        // because a tab of the other bar is current, has no current tab.
        const auto *owner = widget ? qobject_cast<const QTabBar *>(widget->parentWidget()) : nullptr;
        const bool selected = (option->state & State_Selected) && !(owner && !tabbar::selectionShown(*owner));
        bool shown = selected || (option->state & State_MouseOver);
        if (!shown && widget) {
            if (const QTabBar *bar = owner) {
                const int index = tabOfButton(bar, widget);
                shown = index >= 0 && bar->underMouse()
                    && bar->tabRect(index).contains(bar->mapFromGlobal(QCursor::pos()));
            }
        }
        if (!shown)
            return;
        if (option->state & State_MouseOver)
            fillRounded(painter, option->rect, fill(option->palette, (option->state & State_Sunken) ? strongAlpha * 2 : strongAlpha));
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        QColor line = option->palette.color(QPalette::WindowText);
        line.setAlpha(170);
        painter->setPen(QPen(line, 1.3, Qt::SolidLine, Qt::RoundCap));
        const QRectF cross = QRectF(0, 0, 7, 7).translated(QRectF(option->rect).center() - QPointF(3.5, 3.5));
        painter->drawLine(cross.topLeft(), cross.bottomRight());
        painter->drawLine(cross.topRight(), cross.bottomLeft());
        painter->restore();
        return;
    }
    default:
        break;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

QSize AppStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                                 const QWidget *widget) const
{
    // A flat tool button needs no room for a bevel.
    if (type == CT_ToolButton && option && isFlatToolButton(option, widget))
        return size + QSize(10, 8);
    if (type == CT_TabBarTab) {
        QSize tabSize = QProxyStyle::sizeFromContents(type, option, size, widget);
        tabSize.setHeight(qMax(tabSize.height(), size.height() + 12));
        return tabSize;
    }
    return QProxyStyle::sizeFromContents(type, option, size, widget);
}

int AppStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    switch (metric) {
    case PM_TabBarTabOverlap:
        return 0;
    case PM_TabBarTabHSpace:
        return 24;
    case PM_TabCloseIndicatorWidth:
    case PM_TabCloseIndicatorHeight:
        return 16;
    default:
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
}

int AppStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                        QStyleHintReturn *returnData) const
{
    if (hint == SH_TabBar_CloseButtonPosition)
        return QTabBar::RightSide;
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

void AppStyle::polish(QWidget *widget)
{
    QProxyStyle::polish(widget);
    // Tabs and flat buttons draw a hover fill, and a tab bar repaints its
    // close buttons as the mouse moves between tabs.
    if (qobject_cast<QTabBar *>(widget) || qobject_cast<QToolButton *>(widget))
        widget->setAttribute(Qt::WA_Hover);
    if (qobject_cast<QTabBar *>(widget))
        widget->installEventFilter(this);
}

void AppStyle::unpolish(QWidget *widget)
{
    if (qobject_cast<QTabBar *>(widget))
        widget->removeEventFilter(this);
    QProxyStyle::unpolish(widget);
}

bool AppStyle::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::HoverMove:
    case QEvent::HoverLeave:
    case QEvent::Leave:
        if (auto *bar = qobject_cast<QTabBar *>(watched)) {
            const auto side = QTabBar::ButtonPosition(styleHint(SH_TabBar_CloseButtonPosition, nullptr, bar));
            for (int i = 0; i < bar->count(); ++i) {
                if (QWidget *button = bar->tabButton(i, side))
                    button->update();
            }
        }
        break;
    default:
        break;
    }
    return QProxyStyle::eventFilter(watched, event);
}

} // namespace mirante
