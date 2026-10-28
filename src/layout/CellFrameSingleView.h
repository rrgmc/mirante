#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

class QWidget;

namespace mirante {

class SingleViewTracker;

// Private to the layout library. A child object of a CellFrame whose item type
// has singleView on. It claims the item in a SingleViewTracker when the frame
// is shown, and tells the frame whether to show the item or "Shown in another
// cell". LayoutView finds it with findChild(). Implemented in CellFrame.cpp.
class CellFrameSingleView : public QObject
{
    Q_OBJECT

public:
    // showHere is called with true when the frame should show the item, and
    // with false when another cell holds it.
    CellFrameSingleView(QWidget *frame, const QString &itemId, std::function<void(bool)> showHere);
    ~CellFrameSingleView() override;

    void setTracker(SingleViewTracker *tracker);

    // Makes the frame the holder. Called by the "Show here" button.
    void claim();

    // Gives up the item. Called when the frame is destroyed.
    void release();

    // The frame for which this object is the link, or nullptr.
    static CellFrameSingleView *of(QWidget *frame);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void update();

    QWidget *m_frame;
    QString m_itemId;
    std::function<void(bool)> m_showHere;
    QPointer<SingleViewTracker> m_tracker;
    QMetaObject::Connection m_connection;
};

} // namespace mirante
