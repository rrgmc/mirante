#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace mirante {

class Model;

// A file that could not be used, shown in the main window's warning bar.
struct LoadProblem
{
    QString file;       // relative to the data folder, such as "tasks/<id>.json"
    QString message;
};

// Reads the data folder described in docs/sync.md#the-data-folder. The Store
// class below adds writing and watching. Implemented in Store.cpp.
namespace store {

// Loads the folder into the model, replacing its content, and the optional
// workspace.mirante with the workspace's name and web profiles. Accepts schemaVersion 1 only.
// Files that cannot be used are skipped and reported.
// dir may be a resource path such as ":/fixtures".
std::vector<LoadProblem> load(const QString &dir, Model &model);

// The files the model would write: relative path to file content, including
// schemaVersion. Used by the write-back test, and the base of writing in M3.
// workspace.mirante is included when the workspace has a name or web profiles.
std::map<QString, QJsonObject> serialise(const Model &model);

// The content of one file the model would write, such as "tasks/<id>.json" or
// "workspace.mirante", including schemaVersion. nullopt if the entity is gone
// or the path names no entity file. serialise() is built on it.
std::optional<QJsonObject> fileFor(const Model &model, const QString &relPath);

// The bytes written for a file: pretty-printed JSON with sorted keys.
QByteArray toBytes(const QJsonObject &json);

// A workspace is a folder with workspace.mirante at its top. See
// docs/decisions/0027-workspace-needs-its-file.md.
QString workspaceFileName();                // "workspace.mirante"
bool isWorkspace(const QString &dir);
// Makes the folder a workspace by writing its workspace.mirante, with the name.
bool createWorkspace(const QString &dir, const QString &name, QString *error = nullptr);

// Writes the workspace's name into workspace.mirante at the top of the data
// folder, keeping the file's other keys. The first file M2 writes; see
// docs/decisions/0025-workspace-file.md. Returns false and sets *error if it
// cannot.
bool saveWorkspaceName(const QString &dir, const QString &name, QString *error = nullptr);

} // namespace store

// Keeps the model and the data folder in step: loads the folder, writes every
// change through FileWriter, reloads external changes reported by
// FolderWatcher, and lists conflict copies. See docs/sync.md and the M3 plan,
// part 3. Implemented in Store.cpp.
class Store : public QObject
{
    Q_OBJECT

public:
    explicit Store(Model &model, QObject *parent = nullptr);
    ~Store() override;

    // Closes the open folder, loads dir into the model, then watches dir and
    // writes changes. A resource path such as ":/fixtures" opens read-only:
    // edits change the model only. See docs/decisions/0029-sample-data-read-only.md.
    std::vector<LoadProblem> open(const QString &dir);
    // Flushes pending writes and stops watching. The model keeps its content.
    void close();
    QString dir() const;
    bool readOnly() const;
    // Writes pending changes now. Called before quitting.
    void flush();

    // The current problems: those of loading and reloading, and failed writes.
    std::vector<LoadProblem> problems() const;

    // Saves data in files/ under a new name, "<uuid>.<extension>", and returns
    // the name. Returns an empty string and sets *error if it cannot, or if the
    // store is read-only.
    QString saveFile(const QByteArray &data, const QString &extension, QString *error = nullptr);
    // The path of a file in files/, by its name.
    QString filePath(const QString &name) const;

    // A conflict copy made by a sync tool, and the file it is a copy of, both
    // relative to the data folder.
    struct Conflict
    {
        QString file;
        QString copy;
    };
    std::vector<Conflict> conflicts() const;
    // Deletes the copy.
    void keepCurrent(const Conflict &conflict);
    // Writes the copy's content over the file, deletes the copy, and reloads it.
    void useCopy(const Conflict &conflict);

signals:
    void problemsChanged();
    void conflictsChanged();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
