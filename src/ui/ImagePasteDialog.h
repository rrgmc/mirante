#pragma once

#include <QDialog>
#include <QImage>

#include <functional>

class QLabel;
class QPushButton;

namespace mirante {

class ImageCropView;

// The dialog every image paste opens first. It shows the pasted image scaled
// to fit, with its size in pixels, and lets the user crop it with an
// ImageCropView. "Whole Image" removes the crop. "Paste" pastes the cropped
// image; Cancel pastes nothing. Enter pastes and Escape cancels. See docs/ui.md#pasting-images and
// docs/decisions/0045-preview-and-crop-pasted-images.md.
// Implemented in ImagePasteDialog.cpp.
class ImagePasteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImagePasteDialog(const QImage &image, QWidget *parent = nullptr);

    ImageCropView *cropView() const;
    // The crop in the image's pixels, or an empty rectangle for none.
    QRect crop() const;
    void setCrop(const QRect &crop);
    // The pasted image, cropped if there is a crop, at full resolution.
    QImage croppedImage() const;
    // The text that gives the image's size and the crop's.
    QString sizeText() const;

    // Runs a dialog and says whether it was accepted. Callers that open this
    // dialog take one, so tests can work the dialog instead of exec().
    using Exec = std::function<bool(QDialog &dialog)>;
    static Exec defaultExec();

    // Opens the dialog on image through exec. Returns the cropped image, or
    // a null image if the user cancelled.
    static QImage ask(const QImage &image, QWidget *parent, const Exec &exec);

private:
    void updateSize();

    ImageCropView *m_view = nullptr;
    QLabel *m_size = nullptr;
    QPushButton *m_whole = nullptr;
    QPushButton *m_accept = nullptr;
};

} // namespace mirante
