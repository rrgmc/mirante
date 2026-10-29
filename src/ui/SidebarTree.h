#pragma once

#include <QTreeView>

namespace mirante {

// The sidebar's tree view. It paints row backgrounds as rounded shapes inset
// from the edges, as macOS sidebars do: the selection in the highlight colour,
// a light tint of the group colour on project rows, and the hover tint of SidebarDelegate. The
// view and the delegate draw no background of their own. The tree has the
// window colour as its background. Implemented in SidebarTree.cpp.
class SidebarTree : public QTreeView
{
    Q_OBJECT

public:
    explicit SidebarTree(QWidget *parent = nullptr);

protected:
    void drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

} // namespace mirante
