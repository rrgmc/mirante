#pragma once

#include <QObject>
#include <QString>

#include <functional>
#include <memory>

class QWebEngineProfile;

namespace mirante {

// Creates and owns the QWebEngineProfiles that keep web logins: one for each
// profile of the open workspace, by its id, and the app profile. Each
// workspace keeps its own, in a folder of its own. See
// docs/web-views.md#profiles and decisions 0042, 0050 and 0059 in
// docs/decisions/.
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
    // Until setWorkspace() is called, profiles are kept directly in root.
    explicit ProfileManager(const QString &root, QObject *parent = nullptr);
    ~ProfileManager() override;

    // The storage name of a profile's web logins: "p-<id>". An id that is not made of
    // letters, digits, "-" and "_" gives the name of the id "default".
    static QString storageName(const QString &profileId);

    // The profile with this id, made when first asked for. An empty id means
    // the id "default". The manager does not check that the workspace lists the id;
    // profiles::effective() does that.
    QWebEngineProfile *profile(const QString &profileId);
    // "app", for built-in panels from M9.
    QWebEngineProfile *appProfile();

    // Clears the cookies and cache of a profile, then removes its folder once
    // no page uses it. Called only when the user deletes the profile on this
    // device; see docs/decisions/0039-profiles-deleted-only-by-local-delete.md.
    // An id that is not a plain name is never removed: it maps to the folder
    // of the id "default".
    void removeProfile(const QString &profileId);

    // Deletes a profile's logins, cookies, cache and site data on this device,
    // and keeps the profile. Qt cannot clear site storage of a
    // loaded profile, so the profile is released and its folder removed, as
    // for removeProfile(). profileClearing() comes first, so views drop their
    // pages, and profileCleared() at the end, so they can load them again.
    // While it runs, profile() gives a temporary profile that keeps nothing.
    void clearProfile(const QString &profileId);
    bool isClearing(const QString &profileId) const;

    // Removes the folders of the profiles from before decision 0042,
    // "g-<groupId>-<moduleId>" and "shared-<moduleId>". main.cpp calls it at
    // start. Once they are gone, it finds nothing.
    void removeOldProfiles();

    // The data folder of a profile of the open workspace, by its storage
    // name: root/<workspace>/<storageName>.
    QString storagePath(const QString &storageName) const;

    // Chooses the workspace whose profiles profile() gives, by its id. The
    // main window calls it when a workspace opens, before any view asks for a
    // profile. A key that is not a plain name, such as a folder path for a
    // workspace without an id, is hashed. Profiles of the workspace before
    // stay loaded until the manager goes, since pages of it may still close.
    void setWorkspace(const QString &key);
    // The folder name under root for the open workspace; empty before setWorkspace().
    QString workspace() const;

    // Moves the profile folders kept directly in root, from before decision
    // 0059, into the open workspace's folder. The main window calls it when
    // a workspace has just received its id. A folder the workspace already
    // has is left. Returns how many were moved.
    int adoptRootProfiles();
    // Moves the profile folders of the workspace that had workspaceId on this
    // device into the open workspace's folder, after another device gave the
    // workspace a new id. See decision 0062. A folder the workspace already
    // has, or a profile still loaded, is left. Returns how many were moved.
    int adoptProfilesOf(const QString &workspaceId);

signals:
    void profileClearing(const QString &profileId);
    void profileCleared(const QString &profileId);

private:
    // Moves the p-* folders in source into the open workspace's folder.
    int adoptProfilesIn(const QString &source);
    // Removes the profile kept in this folder, loaded or not, and the
    // folder. done runs once the folder is gone, or the attempts ran out.
    void removeFolder(const QString &path, std::function<void()> done = {});

    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
