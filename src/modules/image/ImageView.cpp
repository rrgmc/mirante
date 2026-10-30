#include "ImageView.h"

#include <mirante/ItemContext.h>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QImage>
#include <QImageReader>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QUrl>
#include <QVBoxLayout>

namespace mirante::modules::image {

namespace {

// The link a click opens: a web address, else nothing.
QUrl webLink(const QString &text)
{
    const QUrl url(text.trimmed(), QUrl::StrictMode);
    if (url.isValid() && !url.host().isEmpty()
        && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")))
        return url;
    return {};
}

// A grey message in the middle, as the core's placeholders look, for
// contexts that make no message view.
QWidget *messageLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    label->setForegroundRole(QPalette::PlaceholderText);
    return label;
}

// The image, scaled to fit with its aspect ratio kept, and centred. The
// image is read once; the scaled copy is made again only when the size
// changes.
class Picture : public QWidget
{
public:
    Picture(const QImage &image, const QUrl &link, QWidget *parent)
        : QWidget(parent)
        , m_image(image)
        , m_link(link)
    {
        setObjectName(QStringLiteral("image"));
        // The cell decides the size, not the image.
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        setMinimumSize(16, 16);
        if (!m_link.isEmpty()) {
            setCursor(Qt::PointingHandCursor);
            setToolTip(m_link.toString());
        }
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const qreal ratio = devicePixelRatioF();
        const QSize target = (QSizeF(size()) * ratio).toSize();
        if (m_scaled.isNull() || m_scaledFor != target) {
            m_scaled = QPixmap::fromImage(m_image.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_scaled.setDevicePixelRatio(ratio);
            m_scaledFor = target;
        }
        const QSizeF shown = m_scaled.deviceIndependentSize();
        const QPointF topLeft((width() - shown.width()) / 2.0, (height() - shown.height()) / 2.0);
        QPainter painter(this);
        painter.drawPixmap(topLeft, m_scaled);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        m_pressed = event->button() == Qt::LeftButton;
        QWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        const bool click = m_pressed && event->button() == Qt::LeftButton && rect().contains(event->position().toPoint());
        m_pressed = false;
        if (click && !m_link.isEmpty())
            QDesktopServices::openUrl(m_link);
        QWidget::mouseReleaseEvent(event);
    }

private:
    QImage m_image;
    QUrl m_link;
    QPixmap m_scaled;
    QSize m_scaledFor;
    bool m_pressed = false;
};

} // namespace

QWidget *createImageView(ItemContext &context, QWidget *parent)
{
    const QJsonObject config = context.config();
    const QString fileName = config.value(QStringLiteral("file")).toString();
    const QString path = fileName.isEmpty() ? QString() : context.filePath(fileName);
    if (path.isEmpty()) {
        const QString text = QCoreApplication::translate("image", "The image file is missing.");
        if (QWidget *message = context.createMessageView(text, parent))
            return message;
        return messageLabel(text, parent);
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        const QString text = QCoreApplication::translate("image", "The image file cannot be read.");
        if (QWidget *message = context.createMessageView(text, parent))
            return message;
        return messageLabel(text, parent);
    }

    auto *view = new QWidget(parent);
    auto *layout = new QVBoxLayout(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new Picture(image, webLink(config.value(QStringLiteral("link")).toString()), view), 1);

    auto *memo = new QLabel(view);
    memo->setObjectName(QStringLiteral("memo"));
    memo->setTextFormat(Qt::PlainText);
    memo->setWordWrap(true);
    memo->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    memo->setContentsMargins(6, 2, 6, 4);
    const QString text = config.value(QStringLiteral("memo")).toString();
    memo->setText(text);
    memo->setVisible(!text.trimmed().isEmpty());
    layout->addWidget(memo);
    return view;
}

} // namespace mirante::modules::image
