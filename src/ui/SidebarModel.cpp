// Stub from task T0. Task T11 implements this file.
#include "ui/SidebarModel.h"

namespace mirante {

struct SidebarModel::Private
{
    SortFilter filter;
};

SidebarModel::SidebarModel(const Model & /*model*/, const ItemTypeRegistry & /*types*/, QObject *parent)
    : QAbstractItemModel(parent)
    , d(std::make_unique<Private>())
{
}

SidebarModel::~SidebarModel() = default;

void SidebarModel::setFilter(const SortFilter &filter) { d->filter = filter; }
SortFilter SidebarModel::filter() const { return d->filter; }

QModelIndex SidebarModel::indexOf(const QString & /*id*/) const { return {}; }

QModelIndex SidebarModel::index(int /*row*/, int /*column*/, const QModelIndex & /*parent*/) const { return {}; }
QModelIndex SidebarModel::parent(const QModelIndex & /*child*/) const { return {}; }
int SidebarModel::rowCount(const QModelIndex & /*parent*/) const { return 0; }
int SidebarModel::columnCount(const QModelIndex & /*parent*/) const { return 1; }
QVariant SidebarModel::data(const QModelIndex & /*index*/, int /*role*/) const { return {}; }
Qt::ItemFlags SidebarModel::flags(const QModelIndex &index) const { return QAbstractItemModel::flags(index); }
QStringList SidebarModel::mimeTypes() const { return {}; }
QMimeData *SidebarModel::mimeData(const QModelIndexList & /*indexes*/) const { return nullptr; }

} // namespace mirante
