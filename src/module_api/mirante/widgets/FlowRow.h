#pragma once

#include <QLayout>
#include <QList>

namespace mirante::widgets {

// A layout that puts its widgets in a row and wraps them onto further rows
// when the width runs out, for pills, chips and buttons.
class FlowRow : public QLayout
{
public:
    explicit FlowRow(QWidget *parent = nullptr);
    ~FlowRow() override;

    void addItem(QLayoutItem *item) override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect &rect) override;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;

private:
    QList<QLayoutItem *> m_items;
};

} // namespace mirante::widgets
