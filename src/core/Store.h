#pragma once

#include <QJsonObject>
#include <QString>

#include <map>
#include <vector>

namespace mirante {

class Model;

// A file that could not be used, shown in the main window's warning bar.
struct LoadProblem
{
    QString file;       // relative to the data folder, such as "tasks/<id>.json"
    QString message;
};

// Reads the data folder described in docs/sync.md#the-data-folder.
// In M2 the store only reads. Writing arrives in M3. Implemented in Store.cpp.
namespace store {

// Loads the folder into the model, replacing its content, and the optional
// workspace.mirante with the workspace's name. Accepts schemaVersion 1 only.
// Files that cannot be used are skipped and reported.
// dir may be a resource path such as ":/fixtures".
std::vector<LoadProblem> load(const QString &dir, Model &model);

// The files the model would write: relative path to file content, including
// schemaVersion. Used by the write-back test, and the base of writing in M3.
// workspace.mirante is included when the workspace has a name.
std::map<QString, QJsonObject> serialise(const Model &model);

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

} // namespace mirante
