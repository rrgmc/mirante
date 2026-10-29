#pragma once

#include <QTabBar>

namespace mirante {

// The two fixed slots at the right of the tab row, before the lock: Focus and
// the preview. See decision 0048. They look like tabs. The Focus slot is always
// there and has no close button. The preview slot shows the previewed task
// dashboard or item with an italic title, a pin button and a close button; it
// is hidden while there is no preview. A click on a slot emits slotClicked();
// MainWindow decides what is current, so the bar takes no keyboard focus and
// ignores the mouse wheel. Implemented in SlotTabBar.cpp.
class SlotTabBar : public QTabBar
{
    Q_OBJECT

public:
    enum Slot { FocusSlot = 0, PreviewSlot = 1 };

    explicit SlotTabBar(QWidget *parent = nullptr);
    ~SlotTabBar() override;

    // Shows the preview slot with this title, icon and tooltip.
    void setPreview(const QIcon &icon, const QString &title, const QString &toolTip);
    // Hides the preview slot.
    void clearPreview();
    bool hasPreview() const;

    // Shows a slot as current, or none with -1. The bar keeps a current index
    // for QTabBar's sake, but draws it as selected only while a slot is current.
    void setCurrentSlot(int slot);
    int currentSlot() const;

    QSize tabSizeHint(int index) const override;

signals:
    void slotClicked(int slot);
    void pinRequested();
    void previewCloseRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
};

} // namespace mirante
