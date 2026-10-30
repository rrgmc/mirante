#pragma once

#include <QString>

#include <optional>

class QWidget;

namespace mirante::widgets {

// Asks for the text of a comment. Nothing if the user cancels. Empty text
// cannot be sent.
std::optional<QString> askComment(QWidget *parent, const QString &title);

} // namespace mirante::widgets
