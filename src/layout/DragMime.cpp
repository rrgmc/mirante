#include "layout/DragMime.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>

namespace mirante {

namespace {

const QString refKey = QStringLiteral("ref");
const QString sourceDashboardKey = QStringLiteral("sourceDashboard");
const QString sourcePathKey = QStringLiteral("sourcePath");
const QString taskIdKey = QStringLiteral("taskId");
const QString dashboardIdKey = QStringLiteral("dashboardId");

} // namespace

QMimeData *dragMimeData(const DragPayload &payload)
{
    QJsonObject json;
    json.insert(refKey, cellRefToJson(payload.ref));
    if (payload.sourceDashboard) {
        QJsonObject source;
        source.insert(taskIdKey, payload.sourceDashboard->taskId);
        source.insert(dashboardIdKey, payload.sourceDashboard->dashboardId);
        json.insert(sourceDashboardKey, source);
    }
    if (payload.sourcePath) {
        QJsonArray path;
        for (int index : *payload.sourcePath)
            path.append(index);
        json.insert(sourcePathKey, path);
    }

    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(dragMimeType), QJsonDocument(json).toJson(QJsonDocument::Compact));
    return mime;
}

std::optional<DragPayload> dragPayload(const QMimeData *mime)
{
    const QString type = QString::fromLatin1(dragMimeType);
    if (!mime || !mime->hasFormat(type))
        return std::nullopt;
    const QJsonDocument document = QJsonDocument::fromJson(mime->data(type));
    if (!document.isObject())
        return std::nullopt;
    const QJsonObject json = document.object();
    if (!json.value(refKey).isObject())
        return std::nullopt;

    DragPayload payload;
    payload.ref = cellRefFromJson(json.value(refKey).toObject());

    if (json.contains(sourceDashboardKey)) {
        const QJsonValue value = json.value(sourceDashboardKey);
        if (!value.isObject())
            return std::nullopt;
        const QJsonObject source = value.toObject();
        if (!source.value(dashboardIdKey).isString() || source.value(dashboardIdKey).toString().isEmpty())
            return std::nullopt;
        payload.sourceDashboard =
            DashboardRef{source.value(taskIdKey).toString(), source.value(dashboardIdKey).toString()};
    }

    if (json.contains(sourcePathKey)) {
        const QJsonValue value = json.value(sourcePathKey);
        if (!value.isArray())
            return std::nullopt;
        NodePath path;
        for (const QJsonValue &index : value.toArray()) {
            if (!index.isDouble() || index.toDouble() < 0 || index.toDouble() != index.toInt())
                return std::nullopt;
            path.push_back(index.toInt());
        }
        payload.sourcePath = path;
    }
    return payload;
}

} // namespace mirante
