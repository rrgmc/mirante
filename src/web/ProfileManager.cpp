#include "web/ProfileManager.h"

#include "core/Entities.h"

#include <QDir>
#include <QUrl>

namespace mirante {

// Stub from M4 T0. T1 implements it.
struct ProfileManager::Private
{
    QString root;
};

ProfileManager::ProfileManager(const QString &root, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>(Private{root}))
{
}

ProfileManager::~ProfileManager() = default;

QWebEngineProfile *ProfileManager::groupProfile(const QString &, const QString &) { return nullptr; }

QWebEngineProfile *ProfileManager::sharedProfile(const QString &) { return nullptr; }

QWebEngineProfile *ProfileManager::appProfile() { return nullptr; }

QWebEngineProfile *ProfileManager::profileFor(const Group &, const QString &, const QUrl &) { return nullptr; }

void ProfileManager::removeGroup(const QString &) {}

QString ProfileManager::storagePath(const QString &storageName) const
{
    return QDir(d->root).filePath(storageName);
}

} // namespace mirante
