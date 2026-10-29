#pragma once

#include <QJsonObject>
#include <QString>

#include <functional>
#include <vector>

// Upgrades data folder files from older schemaVersion values. See
// docs/sync.md#the-data-folder and the M3 plan, part 2.
// Implemented in Migrations.cpp.

namespace mirante::migrations {

enum class FileKind { Workspace, Group, Project, Task, RootDashboard };

constexpr int currentSchemaVersion = 1;

// One step upgrades a file from version n to n + 1. Step 0 upgrades version 1.
using Step = std::function<void(FileKind kind, QJsonObject &json)>;

// Upgrades json in place to currentSchemaVersion and sets its schemaVersion.
// Returns false and sets *error for a missing or invalid version, or one newer
// than this app's; json is then unchanged.
bool migrate(FileKind kind, QJsonObject &json, QString *error = nullptr);

// The same with a given list of steps, for tests. The current version is then
// steps.size() + 1.
bool migrate(FileKind kind, QJsonObject &json, const std::vector<Step> &steps, QString *error = nullptr);

} // namespace mirante::migrations
