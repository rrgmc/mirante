#include "ui/ImagePasteDialog.h"

#include "ui/ImageCropView.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace mirante {

ImagePasteDialog::ImagePasteDialog(const QImage &image, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Paste Image"));

    auto *layout = new QVBoxLayout(this);
    auto *hint = new QLabel(tr("Drag on the image to crop it."), this);
    hint->setObjectName(QStringLiteral("cropHint"));
    layout->addWidget(hint);

    m_view = new ImageCropView(this);
    m_view->setObjectName(QStringLiteral("cropView"));
    m_view->setImage(image);
    layout->addWidget(m_view, 1);

    auto *bottom = new QHBoxLayout;
    m_size = new QLabel(this);
    m_size->setObjectName(QStringLiteral("imageSize"));
    bottom->addWidget(m_size, 1);
    m_whole = new QPushButton(tr("Whole Image"), this);
    m_whole->setObjectName(QStringLiteral("wholeImage"));
    m_whole->setAutoDefault(false);
    bottom->addWidget(m_whole);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_accept = buttons->button(QDialogButtonBox::Ok);
    m_accept->setText(tr("Paste"));
    m_accept->setDefault(true);
    buttons->button(QDialogButtonBox::Cancel)->setAutoDefault(false);
    bottom->addWidget(buttons);
    layout->addLayout(bottom);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_whole, &QPushButton::clicked, m_view, &ImageCropView::clearCrop);
    connect(m_view, &ImageCropView::cropChanged, this, &ImagePasteDialog::updateSize);
    updateSize();
}

ImageCropView *ImagePasteDialog::cropView() const { return m_view; }

QRect ImagePasteDialog::crop() const { return m_view->crop(); }

void ImagePasteDialog::setCrop(const QRect &crop) { m_view->setCrop(crop); }

QImage ImagePasteDialog::croppedImage() const
{
    const QRect crop = m_view->crop();
    return crop.isEmpty() ? m_view->image() : m_view->image().copy(crop);
}

QString ImagePasteDialog::sizeText() const { return m_size->text(); }

void ImagePasteDialog::updateSize()
{
    const QSize size = m_view->image().size();
    const QRect crop = m_view->crop();
    if (crop.isEmpty()) {
        m_size->setText(tr("%1 × %2 pixels").arg(size.width()).arg(size.height()));
    } else {
        m_size->setText(tr("Crop: %1 × %2 of %3 × %4 pixels")
                            .arg(crop.width())
                            .arg(crop.height())
                            .arg(size.width())
                            .arg(size.height()));
    }
    m_whole->setEnabled(!crop.isEmpty());
}

ImagePasteDialog::Exec ImagePasteDialog::defaultExec()
{
    return [](QDialog &dialog) { return dialog.exec() == QDialog::Accepted; };
}

QImage ImagePasteDialog::ask(const QImage &image, QWidget *parent, const Exec &exec)
{
    ImagePasteDialog dialog(image, parent);
    if (!(exec ? exec(dialog) : defaultExec()(dialog)))
        return {};
    return dialog.croppedImage();
}

} // namespace mirante
