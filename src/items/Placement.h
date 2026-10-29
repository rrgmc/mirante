#pragma once

#include <QString>
#include <QWidget>

#include <memory>

namespace mirante {

class DeviceSettings;
class ItemTypeRegistry;
class Model;
class ProfileManager;
class ViewPool;

// What placements need. MainWindow owns the objects. See the M4 plan, part 5.
struct PlacementServices
{
    Model *model = nullptr;
    const ItemTypeRegistry *types = nullptr;
    ProfileManager *profiles = nullptr;
    ViewPool *pool = nullptr;
    DeviceSettings *settings = nullptr;
};

// The body of an item's cell: the item's view, made when the placement is
// first shown. It holds the item's ItemContext. A web view is added to the
// ViewPool, and replaced by a "Reload" placeholder while it is unloaded. The
// view is made again when the item's type or configuration changes.
// Implemented in Placement.cpp.
class Placement : public QWidget
{
    Q_OBJECT

public:
    // The services must outlive the placement.
    Placement(const PlacementServices &services, const QString &taskId, const QString &itemId,
              QWidget *parent = nullptr);
    ~Placement() override;

    QString taskId() const;
    QString itemId() const;

    // Null before the first show, while unloaded, and for a type without a view.
    QWidget *view() const;

    // The widget the view puts in the cell header, such as a web view's
    // toolbar, or null.
    QWidget *headerWidget() const;

signals:
    void headerWidgetChanged(QWidget *widget);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
