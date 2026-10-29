#pragma once

#include <QObject>
#include <QString>

#include <memory>

class QWebEngineProfile;

namespace mirante {

// Creates and owns the web login profiles: one for each web profile of the
// workspace, by its id, and the app profile. See docs/web-views.md#profiles
// and docs/decisions/0042-user-managed-web-profiles.md.
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

    // The storage name of a web profile: "p-<id>". An id that is not made of
    // letters, digits, "-" and "_" gives the name of Default.
    static QString storageName(const QString &profileId);

    // The profile with this id, made when first asked for. An empty id means
    // Default. The manager does not check that the workspace lists the id;
    // webprofiles::effective() does that.
    QWebEngineProfile *profile(const QString &profileId);
    // "app", for built-in panels from M9.
    QWebEngineProfile *appProfile();

    // Clears the cookies and cache of a profile, then removes its folder once
    // no page uses it. Called only when the user deletes the profile on this
    // device; see docs/decisions/0039-profiles-deleted-only-by-local-delete.md.
    // Default is never removed.
    void removeProfile(const QString &profileId);

    // Removes the folders of the profiles from before decision 0042,
    // "g-<groupId>-<moduleId>" and "shared-<moduleId>". main.cpp calls it at
    // start. Once they are gone, it finds nothing.
    void removeOldProfiles();

    // The data folder of a profile, by its storage name.
    QString storagePath(const QString &storageName) const;

private:
    // Removes the profile with this storage name, loaded or not, and its folder.
    void removeStorage(const QString &storageName);

    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
