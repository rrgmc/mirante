#include "core/GroupColours.h"

#include "core/Model.h"

#include <QByteArray>

#include <vector>

namespace mirante::groupcolours {

const QStringList &palette()
{
    // Medium colours, far enough apart to tell groups apart. Mixed lightly into
    // the window colour, each gives a tint that keeps text readable in light
    // and dark palettes.
    static const QStringList colours{
        QStringLiteral("#3b82f6"),   // blue
        QStringLiteral("#14b8a6"),   // teal
        QStringLiteral("#8b5cf6"),   // violet
        QStringLiteral("#22c55e"),   // green
        QStringLiteral("#ec4899"),   // pink
        QStringLiteral("#eab308"),   // yellow
        QStringLiteral("#06b6d4"),   // cyan
        QStringLiteral("#6366f1"),   // indigo
    };
    return colours;
}

bool isValid(const QString &text)
{
    if (text.size() != 7 || text.at(0) != QLatin1Char('#'))
        return false;
    for (int i = 1; i < 7; ++i) {
        const QChar c = text.at(i);
        const bool hex = (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
            || (c >= QLatin1Char('a') && c <= QLatin1Char('f')) || (c >= QLatin1Char('A') && c <= QLatin1Char('F'));
        if (!hex)
            return false;
    }
    return true;
}

QString fallback(const QString &groupId)
{
    // FNV-1a over the UTF-8 bytes. qHash() is seeded per run, so it would not
    // give the same colour on every device.
    quint32 hash = 2166136261u;
    for (const char byte : groupId.toLower().toUtf8()) {
        hash ^= quint8(byte);
        hash *= 16777619u;
    }
    return palette().at(int(hash % quint32(palette().size())));
}

QString of(const Group &group)
{
    return isValid(group.colour) ? group.colour.toLower() : fallback(group.id);
}

QString ofTask(const Model &model, const QString &taskId)
{
    const Task *task = model.task(taskId);
    const Project *project = task ? model.project(task->projectId) : nullptr;
    const Group *group = project ? model.group(project->groupId) : nullptr;
    return group ? of(*group) : QString();
}

QString next(const Model &model)
{
    std::vector<int> uses(std::size_t(palette().size()), 0);
    for (const Group *group : model.groups()) {
        const int index = int(palette().indexOf(of(*group)));
        if (index >= 0)
            ++uses[std::size_t(index)];
    }
    int best = 0;
    for (int i = 1; i < int(uses.size()); ++i) {
        if (uses[std::size_t(i)] < uses[std::size_t(best)])
            best = i;
    }
    return palette().at(best);
}

} // namespace mirante::groupcolours
