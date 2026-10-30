#pragma once

#include <mirante/ItemType.h>

namespace mirante {

// The type of an item whose type is not registered: its module is not in
// this build, or its module no longer has the type. Its cells show a
// placeholder that says which. See docs/modules.md#missing-modules and
// decision 0084. Implemented in UnknownItemType.cpp.
class UnknownItemType : public ItemType
{
public:
    explicit UnknownItemType(const QString &id, bool moduleInBuild = false);

    QString id() const override;
    QString name() const override;
    QIcon icon() const override;

    // The part of the id before the first dot, such as "timetrack".
    QString moduleId() const;
    // Whether the module is in this build, so that only the type is gone.
    bool moduleInBuild() const;

private:
    QString m_id;
    bool m_moduleInBuild = false;
};

} // namespace mirante
