#include <mirante/widgets/CommentDialog.h>

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>

namespace mirante::widgets {

std::optional<QString> askComment(QWidget *parent, const QString &title)
{
    // On the heap, because the parent can go while the dialog runs, as when a
    // change of configuration makes the view again.
    QPointer<QDialog> dialog = new QDialog(parent);
    dialog->setWindowTitle(title);

    auto *layout = new QVBoxLayout(dialog);
    auto *text = new QPlainTextEdit(dialog);
    text->setTabChangesFocus(true);
    layout->addWidget(text);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    QPushButton *send = buttons->button(QDialogButtonBox::Ok);
    send->setText(QCoreApplication::translate("mirante::widgets::CommentDialog", "Send"));
    send->setEnabled(false);
    layout->addWidget(buttons);

    QObject::connect(text, &QPlainTextEdit::textChanged, send,
                     [text, send] { send->setEnabled(!text->toPlainText().trimmed().isEmpty()); });
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    text->setFocus();
    const int result = dialog->exec();
    if (!dialog)
        return std::nullopt;
    const QString comment = text->toPlainText();
    delete dialog;
    if (result != QDialog::Accepted)
        return std::nullopt;
    return comment;
}

} // namespace mirante::widgets
