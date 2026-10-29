#pragma once

#include "ui/SidebarModel.h"

#include <QIcon>
#include <QStyledItemDelegate>

namespace mirante {

// Paints the sidebar rows: a coloured priority marker before projects and
// tasks, and Low projects and tasks dimmed. The marker is a rounded square for
// projects and a circle for tasks. Implemented in SidebarDelegate.cpp.
class SidebarDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    enum class MarkerShape { Circle, RoundedSquare };

    explicit SidebarDelegate(QObject *parent = nullptr);

    // The shape of the priority marker for a row of this kind.
    static MarkerShape markerShape(SidebarModel::Kind kind);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;

private:
    QIcon m_projectMarkers[4];  // indexed by Priority
    QIcon m_taskMarkers[4];     // indexed by Priority
};

} // namespace mirante
