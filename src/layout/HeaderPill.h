#pragma once

#include <QIcon>
#include <QLabel>

#include <vector>

namespace mirante {

// A label for cell headers. It cuts its text with an ellipsis to fit its
// width, may show an icon before the text, and may have a rounded background,
// which makes it a "pill". The colours come from the palette when painting,
// so they follow a change between light and dark. See
// docs/decisions/0033-cell-header-pills.md. Implemented in HeaderPill.cpp.
class HeaderPill : public QLabel
{
    Q_OBJECT

public:
    enum class Fill {
        None,       // plain text, such as the item name
        Neutral,    // grey, for the place and for types of missing modules
        Module,     // a calm colour chosen by module id
    };

    explicit HeaderPill(QWidget *parent = nullptr);

    void setIcon(const QIcon &icon);
    QIcon icon() const;

    void setFill(Fill fill, const QString &moduleId = QString());
    Fill fill() const;

    // The background of the pill in palette, or an invalid colour for Fill::None.
    QColor fillColour(const QPalette &palette) const;

    // True if there is nothing to show: no text and no icon.
    bool isBlank() const;

    QSize sizeHint() const override;
    // The icon alone if there is one, otherwise an ellipsis.
    QSize minimumSizeHint() const override;

    // The hue, from 0 to 359, of a module's pills. Built-in modules have
    // fixed hues; any other module gets one from its id.
    static int moduleHue(const QString &moduleId);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int padding() const;
    int iconSize() const;

    QIcon m_icon;
    Fill m_fill = Fill::None;
    QString m_moduleId;
};

// Places header parts in a row, left-aligned, at their preferred widths.
// When the row is too narrow, it shrinks the parts one at a time in the order
// they were given to setShrinkOrder(), each down to its minimum width, and
// then hides them in the same order. Blank HeaderPills take no space.
class HeaderRow : public QWidget
{
public:
    explicit HeaderRow(QWidget *parent = nullptr);

    // The parts, in display order. They become children of the row.
    void setParts(const std::vector<QWidget *> &parts);
    // The parts in the order they shrink: first the one that gives way first.
    void setShrinkOrder(const std::vector<QWidget *> &order);

    // Places the parts again. Call it after a part's text or icon changed:
    // a hidden part does not tell the row by itself.
    void refresh();

    QSize sizeHint() const override;
    // The minimum of the part that shrinks last.
    QSize minimumSizeHint() const override;

protected:
    bool event(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    std::vector<QWidget *> shownParts() const;
    void relayout();

    std::vector<QWidget *> m_parts;
    std::vector<QWidget *> m_shrinkOrder;
};

} // namespace mirante
