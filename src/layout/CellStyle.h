#pragma once

#include <QColor>
#include <QString>

class QPalette;
class QWidget;

// The look shared by item cells and task cells. Implemented in CellStyle.cpp.

namespace mirante::cellstyle {

// Gives a cell header a background that stands out from the cell body.
// strength is how much of colour is mixed into the window colour; an invalid
// colour means the highlight colour. The window colour comes from the
// header's parent, so calling it again replaces the tint instead of adding to
// it.
void tintHeader(QWidget *header, qreal strength, const QColor &colour = QColor());

// How much colour goes into an item cell's header: lighter than a task
// header, which holds cells.
constexpr qreal itemHeaderStrength = 0.12;

// How strongly task dashboard title bars and task cell headers are tinted
// with their group's colour. See docs/decisions/0046-group-colours.md.
constexpr qreal taskHeaderStrength = 0.22;

// The style sheet border of a cell: dashed in the highlight colour in edit
// mode, solid and thicker when the cell has the focus, and a thin, light line
// otherwise. selector is the cell's class, such as "mirante--CellFrame".
QString border(const QString &selector, const QPalette &palette, bool editMode, bool focused);

} // namespace mirante::cellstyle
