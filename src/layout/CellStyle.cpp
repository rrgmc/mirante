#include "layout/CellStyle.h"

#include <QPalette>
#include <QWidget>

namespace mirante::cellstyle {

namespace {

QColor mix(const QColor &a, const QColor &b, qreal amount)
{
    return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * amount),
                            float(a.greenF() + (b.greenF() - a.greenF()) * amount),
                            float(a.blueF() + (b.blueF() - a.blueF()) * amount));
}

} // namespace

void tintHeader(QWidget *header, qreal strength)
{
    QPalette palette = header->palette();
    palette.setColor(QPalette::Window,
                     mix(palette.color(QPalette::Window), palette.color(QPalette::Highlight), strength));
    header->setPalette(palette);
    header->setBackgroundRole(QPalette::Window);
    header->setAutoFillBackground(true);
}

QString border(const QString &selector, const QPalette &palette, bool editMode, bool focused)
{
    QString line;
    if (focused)
        line = QStringLiteral("2px solid %1").arg(palette.color(QPalette::Highlight).name());
    else if (editMode)
        line = QStringLiteral("2px dashed %1").arg(palette.color(QPalette::Highlight).name());
    else
        line = QStringLiteral("1px solid %1").arg(palette.color(QPalette::Mid).name());
    return QStringLiteral("%1 { border: %2; border-radius: 3px; }").arg(selector, line);
}

} // namespace mirante::cellstyle
