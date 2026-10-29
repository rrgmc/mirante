#pragma once

#include <QLineEdit>

namespace mirante {

// The current address in a web toolbar. It is read-only, and cut in the
// middle with an ellipsis to fit its width. When it has focus it shows the
// whole address, selected, so it can be copied. Implemented in AddressField.cpp.
class AddressField : public QLineEdit
{
    Q_OBJECT

public:
    explicit AddressField(QWidget *parent = nullptr);

    void setAddress(const QString &address);
    QString address() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateText();

    QString m_address;
};

} // namespace mirante
