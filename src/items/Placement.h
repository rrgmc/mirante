#pragma once

#include "items/CellStatus.h"

#include <QString>
#include <QWidget>

#include <functional>
#include <memory>

namespace mirante {

class Attention;
class DeviceSettings;
class GitStatusReader;
class ItemTypeRegistry;
class Model;
class ProfileManager;
class ServerClients;
class StatusCache;
class Store;
class ViewPool;

// What placements need. MainWindow owns the objects. See the M4 plan, part 5.
struct PlacementServices
{
    Model *model = nullptr;
    const ItemTypeRegistry *types = nullptr;
    ProfileManager *profiles = nullptr;
    ViewPool *pool = nullptr;
    DeviceSettings *settings = nullptr;
    // The status of items, and their servers. Null turns status off, as in
    // most tests. See the M5 plan, part 8.
    StatusCache *status = nullptr;
    Attention *attention = nullptr;
    ServerClients *servers = nullptr;
    // Runs git status for ItemContext::gitStatus(). Null: it never answers,
    // as in most tests. See decision 0080.
    GitStatusReader *git = nullptr;
    // The data folder, for ItemContext::filePath(). Null: no files.
    const Store *store = nullptr;
    // Opens the settings screen of a module's server in a profile, for the
    // fix button of "Token not set" and the like. Null leaves it out.
    std::function<void(const QString &profileId, const QString &moduleId)> openServerSettings;
};

// The body of an item's cell: the item's view, made when the placement is
// first shown. It holds the item's ItemContext. A web view is added to the
// ViewPool, and replaced by a "Reload" placeholder while it is unloaded. A
// native view's showPage() shows a page in place of the view; see decision
// 0069. The view is made again when the item's type, configuration or profile changes,
// or the module settings of its profile.
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

    // What the cell header shows about the item's status. Kind::None for a
    // type without status, or without a status cache.
    CellStatus cellStatus() const;
    // True for a type with status: the cell header then shows a refresh button.
    bool hasStatus() const;

public slots:
    // Fetches the item's status at once.
    void refresh();
    // Opens the settings screen of the item's server in its profile.
    void openSettings();

signals:
    void headerWidgetChanged(QWidget *widget);
    void cellStatusChanged(const mirante::CellStatus &status);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
