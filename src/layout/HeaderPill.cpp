#include "layout/HeaderPill.h"

#include <QEvent>
#include <QHash>
#include <QPainter>
#include <QStyle>

#include <algorithm>

namespace mirante {

namespace {

QColor mix(const QColor &a, const QColor &b, qreal amount)
{
    return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * amount),
                            float(a.greenF() + (b.greenF() - a.greenF()) * amount),
                            float(a.blueF() + (b.blueF() - a.blueF()) * amount));
}

bool isDark(const QPalette &palette)
{
    return palette.color(QPalette::Window).lightness() < 128;
}

const QString ellipsis = QString(QChar(0x2026));

// Space between the icon and the text, and between parts of a row.
constexpr int iconGap = 3;
constexpr int partGap = 6;

} // namespace

HeaderPill::HeaderPill(QWidget *parent)
    : QLabel(parent)
{
    setTextFormat(Qt::PlainText);
}

void HeaderPill::setIcon(const QIcon &icon)
{
    m_icon = icon;
    updateGeometry();
    update();
}

QIcon HeaderPill::icon() const { return m_icon; }

void HeaderPill::setFill(Fill fill, const QString &moduleId)
{
    m_fill = fill;
    m_moduleId = moduleId;
    updateGeometry();
    update();
}

HeaderPill::Fill HeaderPill::fill() const { return m_fill; }

// A colour of medium lightness and saturation is mixed into the header's
// background: a little in a light palette, a little more in a dark one, where
// the same amount reads weaker. The text keeps the palette's text colour.
QColor HeaderPill::fillColour(const QPalette &palette) const
{
    const QColor window = palette.color(QPalette::Window);
    switch (m_fill) {
    case Fill::None:
        return {};
    case Fill::Neutral:
        return mix(window, palette.color(QPalette::WindowText), isDark(palette) ? 0.16 : 0.10);
    case Fill::Module:
        return mix(window, QColor::fromHsl(moduleHue(m_moduleId), 140, 128), isDark(palette) ? 0.38 : 0.24);
    }
    return {};
}

bool HeaderPill::isBlank() const { return text().isEmpty() && m_icon.isNull(); }

int HeaderPill::moduleHue(const QString &moduleId)
{
    // Hues far enough apart that the built-in modules are told apart.
    static const QHash<QString, int> hues = {
        {QStringLiteral("gitlab"), 25},     // orange
        {QStringLiteral("notes"), 50},      // amber
        {QStringLiteral("folder"), 120},    // green
        {QStringLiteral("web"), 185},       // teal
        {QStringLiteral("jira"), 215},      // blue
        {QStringLiteral("claude"), 270},    // violet
        {QStringLiteral("image"), 330},     // rose
    };
    const auto it = hues.constFind(moduleId);
    if (it != hues.constEnd())
        return *it;
    return int(qHash(moduleId, 0) % 12) * 30;
}

int HeaderPill::padding() const { return m_fill == Fill::None ? 0 : 5; }

int HeaderPill::iconSize() const
{
    if (m_icon.isNull())
        return 0;
    return std::min(style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this), fontMetrics().height());
}

QSize HeaderPill::sizeHint() const
{
    const QFontMetrics metrics = fontMetrics();
    int width = 2 * padding() + iconSize();
    if (!text().isEmpty())
        width += (iconSize() > 0 ? iconGap : 0) + metrics.horizontalAdvance(text());
    const int height = std::max(metrics.height(), iconSize()) + (m_fill == Fill::None ? 0 : 2);
    return {width, height};
}

QSize HeaderPill::minimumSizeHint() const
{
    const QSize hint = sizeHint();
    if (iconSize() > 0)
        return {2 * padding() + iconSize(), hint.height()};
    if (text().isEmpty())
        return {0, hint.height()};
    return {std::min(hint.width(), 2 * padding() + fontMetrics().horizontalAdvance(ellipsis)), hint.height()};
}

void HeaderPill::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QRect area = contentsRect();
    const QFontMetrics metrics = fontMetrics();
    const int pad = padding();
    const int icon = iconSize();

    // The text that fits. With an icon, text that does not fit is dropped,
    // and the pill shrinks around the icon.
    int x = area.left() + pad;
    const int textLeft = x + icon + (icon > 0 ? iconGap : 0);
    QString shown = metrics.elidedText(text(), Qt::ElideRight, std::max(0, area.right() - pad - textLeft + 1));
    if (icon > 0 && (shown.isEmpty() || shown == ellipsis))
        shown.clear();
    const int contentRight = shown.isEmpty() ? x + icon : textLeft + metrics.horizontalAdvance(shown);
    const int pillWidth = std::min(area.width(), contentRight + pad - area.left());

    const QColor background = fillColour(palette());
    if (background.isValid()) {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(background);
        const QRectF pill(area.left(), area.top(), pillWidth, area.height());
        const qreal radius = std::min<qreal>(pill.height() / 2.0, 6.0);
        painter.drawRoundedRect(pill, radius, radius);
    }

    if (icon > 0) {
        const QRect iconRect(x, area.top() + (area.height() - icon) / 2, icon, icon);
        m_icon.paint(&painter, iconRect, Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled);
    }
    if (!shown.isEmpty()) {
        painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, foregroundRole()));
        painter.drawText(QRect(textLeft, area.top(), area.right() - textLeft + 1, area.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, shown);
    }
}

// HeaderRow

HeaderRow::HeaderRow(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void HeaderRow::setParts(const std::vector<QWidget *> &parts)
{
    m_parts = parts;
    for (QWidget *part : m_parts)
        part->setParent(this);
    refresh();
}

void HeaderRow::setShrinkOrder(const std::vector<QWidget *> &order)
{
    m_shrinkOrder = order;
    relayout();
}

void HeaderRow::refresh()
{
    updateGeometry();
    relayout();
}

std::vector<QWidget *> HeaderRow::shownParts() const
{
    std::vector<QWidget *> shown;
    for (QWidget *part : m_parts) {
        const auto *pill = qobject_cast<const HeaderPill *>(part);
        if (!pill || !pill->isBlank())
            shown.push_back(part);
    }
    return shown;
}

QSize HeaderRow::sizeHint() const
{
    int width = 0;
    int height = 0;
    const std::vector<QWidget *> shown = shownParts();
    for (QWidget *part : shown) {
        const QSize hint = part->sizeHint();
        width += hint.width();
        height = std::max(height, hint.height());
    }
    if (!shown.empty())
        width += partGap * int(shown.size() - 1);
    return {width, height};
}

QSize HeaderRow::minimumSizeHint() const
{
    const std::vector<QWidget *> shown = shownParts();
    int width = 0;
    for (auto it = m_shrinkOrder.rbegin(); it != m_shrinkOrder.rend(); ++it) {
        if (std::find(shown.begin(), shown.end(), *it) != shown.end()) {
            width = (*it)->minimumSizeHint().width();
            break;
        }
    }
    return {width, sizeHint().height()};
}

bool HeaderRow::event(QEvent *event)
{
    // A part changed its text or icon.
    if (event->type() == QEvent::LayoutRequest)
        refresh();
    return QWidget::event(event);
}

void HeaderRow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayout();
}

void HeaderRow::relayout()
{
    const std::vector<QWidget *> shown = shownParts();
    for (QWidget *part : m_parts) {
        const bool visible = std::find(shown.begin(), shown.end(), part) != shown.end();
        if (part->isVisibleTo(this) != visible)
            part->setVisible(visible);
    }
    if (shown.empty())
        return;

    std::vector<int> widths;
    int total = partGap * int(shown.size() - 1);
    for (QWidget *part : shown) {
        widths.push_back(part->sizeHint().width());
        total += widths.back();
    }
    const auto indexOf = [&shown](QWidget *part) {
        return int(std::find(shown.begin(), shown.end(), part) - shown.begin());
    };

    // First each part shrinks to its minimum, then each is dropped, both in
    // the shrink order.
    int over = total - width();
    for (QWidget *part : m_shrinkOrder) {
        const int i = indexOf(part);
        if (over <= 0 || i == int(shown.size()))
            continue;
        const int cut = std::min(over, widths[std::size_t(i)] - part->minimumSizeHint().width());
        if (cut > 0) {
            widths[std::size_t(i)] -= cut;
            over -= cut;
        }
    }
    for (QWidget *part : m_shrinkOrder) {
        const int i = indexOf(part);
        if (over <= 0 || i == int(shown.size()) || part == m_shrinkOrder.back())
            continue;
        over -= widths[std::size_t(i)] + partGap;
        widths[std::size_t(i)] = 0;
    }

    int x = 0;
    for (std::size_t i = 0; i < shown.size(); ++i) {
        const int height = std::min(shown[i]->sizeHint().height(), this->height());
        const int width = std::max(0, std::min(widths[i], this->width() - x));
        shown[i]->setGeometry(x, (this->height() - height) / 2, width, height);
        if (widths[i] > 0)
            x += widths[i] + partGap;
    }
}

} // namespace mirante
