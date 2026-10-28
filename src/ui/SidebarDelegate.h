#pragma once

#include <QIcon>
#include <QStyledItemDelegate>

namespace mirante {

// Paints the sidebar rows: a coloured priority marker before projects and
// tasks, and Low projects and tasks dimmed. Implemented in SidebarDelegate.cpp.
class SidebarDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit SidebarDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;

private:
    QIcon m_markers[4];     // indexed by Priority
};

} // namespace mirante
