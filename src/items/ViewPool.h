#pragma once

#include <QObject>

#include <functional>
#include <memory>

namespace mirante {

// Applies the memory policy of docs/web-views.md#memory-policy. Placements with
// heavy views report when they are shown and hidden. When more hidden views
// are loaded than the limit allows, those hidden longest are unloaded.
// Implemented in ViewPool.cpp.
class ViewPool : public QObject
{
    Q_OBJECT

public:
    explicit ViewPool(QObject *parent = nullptr);
    ~ViewPool() override;

    // The number of hidden views kept loaded. 8 by default.
    void setLimit(int hiddenViews);
    int limit() const;

    // Adds a placement whose view is loaded, as visible. unload is called when
    // the pool unloads it; the pool then forgets it, and the placement adds
    // itself again when it loads its view again.
    void add(QObject *placement, std::function<void()> unload);
    void remove(QObject *placement);
    void setVisible(QObject *placement, bool visible);

    // Loaded placements that are hidden.
    int hiddenCount() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
