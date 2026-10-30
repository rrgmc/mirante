#pragma once

#include <QMetaType>
#include <QString>

namespace mirante {

// What an item cell's header shows about the item's status. A Placement
// builds it from the status cache, and CellFrame shows it. It lives here,
// below the layout library, so placements can make it. See the M5 plan,
// part 8.
struct CellStatus
{
    enum class Kind {
        None,       // nothing to show
        Attention,  // a round badge with the count of reasons that count
        Problem,    // a warning pill, such as "Token not set"
        Stale,      // grey text with the data's age, such as "12 min ago"
    };

    Kind kind = Kind::None;
    int count = 0;          // Attention: the reasons that count
    QString text;           // the short text: the pill's or the age
    QString tooltip;        // the reasons, or the message and the data's age
    bool canFix = false;    // Problem: the pill is a button that emits fixRequested

    bool operator==(const CellStatus &other) const
    {
        return kind == other.kind && count == other.count && text == other.text && tooltip == other.tooltip
            && canFix == other.canFix;
    }
    bool operator!=(const CellStatus &other) const { return !(*this == other); }
};

} // namespace mirante

Q_DECLARE_METATYPE(mirante::CellStatus)
