// Stub from task T0. Task T3 implements this file.
#include "core/Entities.h"

namespace mirante {

std::optional<Folder> Folder::fromJson(const QJsonObject & /*json*/, QString * /*error*/) { return std::nullopt; }
QJsonObject Folder::toJson() const { return {}; }

std::optional<Connection> Connection::fromJson(const QJsonObject & /*json*/, QString * /*error*/) { return std::nullopt; }
QJsonObject Connection::toJson() const { return {}; }

std::optional<Group> Group::fromJson(const QJsonObject & /*json*/, QString * /*error*/) { return std::nullopt; }
QJsonObject Group::toJson() const { return {}; }

std::optional<Project> Project::fromJson(const QJsonObject & /*json*/, QString * /*error*/) { return std::nullopt; }
QJsonObject Project::toJson() const { return {}; }

std::optional<Item> Item::fromJson(const QJsonObject & /*json*/, QString * /*error*/) { return std::nullopt; }
QJsonObject Item::toJson() const { return {}; }

std::optional<Dashboard> Dashboard::fromJson(const QJsonObject & /*json*/, QString * /*error*/) { return std::nullopt; }
QJsonObject Dashboard::toJson() const { return {}; }

std::optional<RootDashboard> RootDashboard::fromJson(const QJsonObject & /*json*/, QString * /*error*/) { return std::nullopt; }
QJsonObject RootDashboard::toJson() const { return {}; }

std::optional<Task> Task::fromJson(const QJsonObject & /*json*/, QString * /*error*/) { return std::nullopt; }
QJsonObject Task::toJson() const { return {}; }

} // namespace mirante
