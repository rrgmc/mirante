#pragma once

#include <QProxyStyle>

namespace mirante {

// The application style: the platform style, with flat tool buttons and tabs.
//
// Qt's platform styles draw these controls in very different ways. The macOS
// style frames every tool button and fills the current tab with the accent
// colour, which looks out of place on a Mac. This style draws them the same
// way on every platform, as quiet rounded shapes that show on hover. Every
// other control keeps the platform look. See docs/ui.md#look.
//
// - An auto-raise tool button outside a toolbar is flat. A rounded fill shows
//   when the mouse is over it, and a stronger one when it is pressed or on.
// - A tab is a rounded fill, light when the mouse is over it and stronger
//   when it is the current tab. Its text keeps the normal text colour.
// - A tab's close button is on the right. It shows on the current tab and on
//   the tab under the mouse only.
// - Form layouts fill their dialog and sit at its top left, as on Windows and
//   Linux, instead of keeping fields at their natural width in a centred form.
class AppStyle : public QProxyStyle
{
    Q_OBJECT

public:
    AppStyle();

    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter,
                            const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget = nullptr) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                           const QWidget *widget) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr,
                  QStyleHintReturn *returnData = nullptr) const override;

    void polish(QWidget *widget) override;
    void unpolish(QWidget *widget) override;
    using QProxyStyle::polish;
    using QProxyStyle::unpolish;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};

} // namespace mirante
