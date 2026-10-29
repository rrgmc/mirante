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

    // The task or item whose row gets the hover tint, or empty. See
    // Sidebar::setHoverHighlight().
    void setHoverId(const QString &id);
    QString hoverId() const;
    bool isHoverTinted(const QModelIndex &index) const;

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;

private:
    QIcon m_markers[4];     // indexed by Priority
    QString m_hoverId;
};

} // namespace mirante
