#pragma once

class QAbstractButton;

namespace mirante::widgets {

// Makes a button look like a link, as the links in a list's rows do: the
// palette's link colour, no frame, and an underline under the mouse or the
// keyboard's focus.
void styleAsLink(QAbstractButton *button);

} // namespace mirante::widgets
