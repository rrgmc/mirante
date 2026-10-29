#include "web/AddressField.h"

#include <QFocusEvent>
#include <QTimer>

#include <algorithm>

namespace mirante {

namespace {

// QLineEdit keeps a few pixels beside its text, and the text must fit
// between them or it scrolls.
constexpr int kInnerMargin = 6;
constexpr int kPreferredWidth = 240;
constexpr int kMinimumWidth = 60;

} // namespace

AddressField::AddressField(QWidget *parent)
    : QLineEdit(parent)
{
    setReadOnly(true);
    setFrame(false);
    // The field looks like a label in the cell header until it is clicked.
    QPalette palette = this->palette();
    palette.setColor(QPalette::Base, Qt::transparent);
    setPalette(palette);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void AddressField::setAddress(const QString &address)
{
    m_address = address;
    setToolTip(address);
    updateText();
}

QString AddressField::address() const { return m_address; }

QSize AddressField::sizeHint() const
{
    QSize size = QLineEdit::sizeHint();
    size.setWidth(kPreferredWidth);
    return size;
}

QSize AddressField::minimumSizeHint() const
{
    QSize size = QLineEdit::minimumSizeHint();
    size.setWidth(kMinimumWidth);
    return size;
}

void AddressField::focusInEvent(QFocusEvent *event)
{
    QLineEdit::focusInEvent(event);
    updateText();
    // A click that gives focus would clear a selection made now, so select
    // after it.
    QTimer::singleShot(0, this, [this] {
        if (hasFocus())
            selectAll();
    });
}

void AddressField::focusOutEvent(QFocusEvent *event)
{
    QLineEdit::focusOutEvent(event);
    updateText();
}

void AddressField::resizeEvent(QResizeEvent *event)
{
    QLineEdit::resizeEvent(event);
    updateText();
}

void AddressField::updateText()
{
    if (hasFocus()) {
        if (text() != m_address)
            setText(m_address);
        return;
    }
    const QMargins margins = textMargins();
    const int width = contentsRect().width() - margins.left() - margins.right() - kInnerMargin;
    setText(fontMetrics().elidedText(m_address, Qt::ElideMiddle, std::max(width, 0)));
    setCursorPosition(0);
}

} // namespace mirante
