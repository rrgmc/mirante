#pragma once

#include <QDialog>

#include <memory>

class QWebEngineProfile;

namespace mirante {

class WebPage;

// The dialog for a login popup, such as an OAuth sign-in window. Its page is
// in the same profile as the page that opened it. It closes, and deletes
// itself, when the page calls window.close(). See
// docs/web-views.md#page-behaviour. Implemented in LoginDialog.cpp.
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    // requested is the size the page asked for, or an empty size.
    LoginDialog(QWebEngineProfile *profile, const QSize &requested, QWidget *parent = nullptr);
    ~LoginDialog() override;

    WebPage *page() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

// The size of a login dialog: the size the page asks for, but at least
// 500 × 600 pixels, and no larger than the screen's available size.
// Pure, for tests.
QSize loginDialogSize(const QSize &requested, const QSize &available);

} // namespace mirante
