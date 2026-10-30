#include <mirante/widgets/CommentDialog.h>

namespace mirante::widgets {

std::optional<QString> askComment(QWidget *parent, const QString &title)
{
    Q_UNUSED(parent)
    Q_UNUSED(title)
    return std::nullopt;
}

} // namespace mirante::widgets
