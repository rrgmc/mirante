#pragma once

#include <QColor>
#include <QString>

class QPalette;
class QWidget;

// The look shared by item cells and task cells. Implemented in CellStyle.cpp.

namespace mirante::cellstyle {

// Gives a cell header a background that stands out from the cell body.
// strength is how much of the highlight colour is mixed into the window colour.
void tintHeader(QWidget *header, qreal strength);

// The style sheet border of a cell: dashed in the highlight colour in edit
// mode, solid and thicker when the cell has the focus, and a thin, light line
// otherwise. selector is the cell's class, such as "mirante--CellFrame".
QString border(const QString &selector, const QPalette &palette, bool editMode, bool focused);

} // namespace mirante::cellstyle
