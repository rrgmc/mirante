#pragma once

#include <QObject>
#include <QString>

#include <memory>

class QUrl;
class QWebEngineProfile;

namespace mirante {

struct Group;

// Creates and owns the web login profiles: one per group and module, one
// shared profile per module, and the app profile. See docs/web-views.md#profiles
// and docs/decisions/0040-profiles-per-group-and-module.md.
// Implemented in ProfileManager.cpp.
//
// A profile must outlive every page that uses it, so the manager is destroyed
// after all web views.
class ProfileManager : public QObject
{
    Q_OBJECT

public:
    // root is the folder that holds every profile's data. main.cpp passes
    // QStandardPaths::AppLocalDataLocation + "/profiles"; tests a temporary folder.
    explicit ProfileManager(const QString &root, QObject *parent = nullptr);
    ~ProfileManager() override;

    // "g-<groupId>-<moduleId>", made when first asked for.
    QWebEngineProfile *groupProfile(const QString &groupId, const QString &moduleId);
    // "shared-<moduleId>".
    QWebEngineProfile *sharedProfile(const QString &moduleId);
    // "app", for built-in panels from M9.
    QWebEngineProfile *appProfile();

    // The module's profile in the group, or the module's shared one if the
    // group's profileOverrides sends the URL's host there. Hosts match
    // exactly, ignoring case.
    QWebEngineProfile *profileFor(const Group &group, const QString &moduleId, const QUrl &url);

    // Clears the cookies and cache of every module's profile of the group,
    // then removes their folders once no page uses them. The folders are found
    // by their "g-<groupId>-" prefix, so profiles of modules missing from this
    // build go too. Called only when the user deletes a group on this device;
    // see docs/decisions/0039-profiles-deleted-only-by-local-delete.md.
    void removeGroup(const QString &groupId);

    // The data folder of a profile, by its storage name.
    QString storagePath(const QString &storageName) const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
