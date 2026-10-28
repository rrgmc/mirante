#pragma once

#include <mirante/ItemType.h>

namespace mirante {

// The type of an item whose module is not in this build. Its cells show the
// missing-module placeholder. See docs/modules.md#missing-modules.
// Implemented in UnknownItemType.cpp.
class UnknownItemType : public ItemType
{
public:
    explicit UnknownItemType(const QString &id);

    QString id() const override;
    QString name() const override;
    QIcon icon() const override;

    // The part of the id before the first dot, such as "timetrack".
    QString moduleId() const;

private:
    QString m_id;
};

} // namespace mirante
