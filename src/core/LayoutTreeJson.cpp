#include "core/LayoutTree.h"

#include <QJsonArray>
#include <QJsonValue>

#include <cmath>

namespace mirante {

namespace {

const QString TypeKey = QStringLiteral("type");
const QString RefKey = QStringLiteral("ref");
const QString DirKey = QStringLiteral("dir");
const QString SizesKey = QStringLiteral("sizes");
const QString ChildrenKey = QStringLiteral("children");
const QString TabsKey = QStringLiteral("tabs");
const QString TitleKey = QStringLiteral("title");
const QString NodeKey = QStringLiteral("node");
const QString CurrentKey = QStringLiteral("current");
const QString CollapseSingleKey = QStringLiteral("collapseSingle");

LayoutNode nodeFromValue(const QJsonValue &value)
{
    // A child that is not an object is an empty cell.
    return value.isObject() ? layoutFromJson(value.toObject()) : LayoutNode{};
}

// Sizes must be one positive number per child. Otherwise all children get
// the same size.
std::vector<double> readSizes(const QJsonValue &value, std::size_t count)
{
    std::vector<double> sizes;
    const QJsonArray array = value.toArray();
    if (static_cast<std::size_t>(array.size()) == count) {
        for (qsizetype i = 0; i < array.size(); ++i) {
            const QJsonValue size = array.at(i);
            if (!size.isDouble() || !(size.toDouble() > 0) || !std::isfinite(size.toDouble()))
                break;
            sizes.push_back(size.toDouble());
        }
    }
    if (sizes.size() != count)
        sizes.assign(count, 1.0);
    return sizes;
}

} // namespace

CellRef cellRefFromJson(const QJsonObject &json)
{
    CellRef ref;
    ref.item = json.value(QStringLiteral("item")).toString();
    ref.task = json.value(QStringLiteral("task")).toString();
    ref.dashboard = json.value(QStringLiteral("dashboard")).toString();
    return ref;
}

QJsonObject cellRefToJson(const CellRef &ref)
{
    QJsonObject json;
    if (!ref.item.isEmpty())
        json.insert(QStringLiteral("item"), ref.item);
    if (!ref.task.isEmpty())
        json.insert(QStringLiteral("task"), ref.task);
    if (!ref.dashboard.isEmpty())
        json.insert(QStringLiteral("dashboard"), ref.dashboard);
    return json;
}

// Known fields with unusable values are repaired, not kept, so that later
// edits of the node are not overwritten on write. A node of an unknown type
// becomes an empty cell that keeps the whole original object, type included,
// in extra. It is written back unchanged until an edit gives it a ref.
LayoutNode layoutFromJson(const QJsonObject &json)
{
    LayoutNode node;
    const QString type = json.value(TypeKey).toString();
    QJsonObject extra = json;
    extra.remove(TypeKey);

    if (type == QLatin1String("split")) {
        node.kind = LayoutNode::Kind::Split;
        node.dir = json.value(DirKey).toString() == QLatin1String("v") ? Qt::Vertical : Qt::Horizontal;
        const QJsonArray children = json.value(ChildrenKey).toArray();
        for (qsizetype i = 0; i < children.size(); ++i)
            node.children.push_back(nodeFromValue(children.at(i)));
        node.sizes = readSizes(json.value(SizesKey), node.children.size());
        extra.remove(DirKey);
        extra.remove(SizesKey);
        extra.remove(ChildrenKey);
    } else if (type == QLatin1String("tabs")) {
        node.kind = LayoutNode::Kind::Tabs;
        const QJsonArray tabs = json.value(TabsKey).toArray();
        for (qsizetype i = 0; i < tabs.size(); ++i) {
            const QJsonObject object = tabs.at(i).toObject();
            node.titles.push_back(object.value(TitleKey).toString());
            node.children.push_back(nodeFromValue(object.value(NodeKey)));
        }
        const QJsonValue current = json.value(CurrentKey);
        const double index = current.toDouble();
        const double count = static_cast<double>(node.children.size());
        if (current.isDouble() && std::floor(index) == index && index >= 0 && index < count)
            node.current = static_cast<int>(index);
        node.collapseSingle = json.value(CollapseSingleKey).toBool();
        extra.remove(TabsKey);
        extra.remove(CurrentKey);
        extra.remove(CollapseSingleKey);
    } else if (type == QLatin1String("cell")) {
        const QJsonValue ref = json.value(RefKey);
        if (ref.isObject()) {
            const CellRef cellRef = cellRefFromJson(ref.toObject());
            if (cellRef != CellRef{})
                node.ref = cellRef;
        }
        extra.remove(RefKey);
    } else {
        extra = json;
    }

    node.extra = extra;
    return node;
}

QJsonObject layoutToJson(const LayoutNode &node)
{
    QJsonObject json;
    switch (node.kind) {
    case LayoutNode::Kind::Split: {
        QJsonArray sizes;
        QJsonArray children;
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            sizes.append(i < node.sizes.size() ? node.sizes[i] : 1.0);
            children.append(layoutToJson(node.children[i]));
        }
        json.insert(TypeKey, QStringLiteral("split"));
        json.insert(DirKey, node.dir == Qt::Vertical ? QStringLiteral("v") : QStringLiteral("h"));
        json.insert(SizesKey, sizes);
        json.insert(ChildrenKey, children);
        break;
    }
    case LayoutNode::Kind::Tabs: {
        QJsonArray tabs;
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            tabs.append(QJsonObject{
                {TitleKey, i < node.titles.size() ? node.titles[i] : QString()},
                {NodeKey, layoutToJson(node.children[i])},
            });
        }
        json.insert(TypeKey, QStringLiteral("tabs"));
        json.insert(TabsKey, tabs);
        json.insert(CurrentKey, node.current);
        json.insert(CollapseSingleKey, node.collapseSingle);
        break;
    }
    case LayoutNode::Kind::Cell:
        if (node.extra.contains(TypeKey)) {
            // A node of an unknown type: written as read, or replaced by the edit.
            if (!node.ref)
                return node.extra;
            return QJsonObject{{TypeKey, QStringLiteral("cell")}, {RefKey, cellRefToJson(*node.ref)}};
        }
        json.insert(TypeKey, QStringLiteral("cell"));
        json.insert(RefKey, node.ref ? QJsonValue(cellRefToJson(*node.ref)) : QJsonValue(QJsonValue::Null));
        break;
    }
    for (auto it = node.extra.begin(); it != node.extra.end(); ++it)
        json.insert(it.key(), it.value());
    return json;
}

} // namespace mirante
