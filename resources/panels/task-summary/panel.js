// Task summary: the cached status of every item of the task that reports
// one, in the task's order. A click opens the item. See
// docs/item-types.md#built-in-panels.
"use strict";

// The second line of a row: the summary, else why there is none.
function detail(status, texts) {
    if (status.state === "ok")
        return dash.element("div", "detail", status.summary);
    if (status.state === "none" || !status.state)
        return dash.element("div", "detail muted", texts.notFetched);
    return dash.element("div", "detail muted", status.message);
}

function itemRow(bridge, item, status, texts) {
    const row = dash.clickable(dash.element("li", "row"), () => bridge.openItem(item.id));
    row.dataset.id = item.id;
    if (status.needsAttention)
        row.classList.add("attention");

    const line = dash.element("div", "line");
    if (item.typeName)
        line.append(dash.element("span", "pill", item.typeName));
    line.append(dash.element("span", "name", item.name));
    if (status.closed === true)
        line.append(dash.element("span", "side", texts.closed));
    if (status.updated)
        line.append(dash.element("span", "side", status.updated));
    row.append(line, detail(status, texts));

    if (status.attention && status.attention.length > 0) {
        const reasons = dash.element("ul", "reasons");
        for (const text of status.attention)
            reasons.append(dash.element("li", "", text));
        row.append(reasons);
    }
    return row;
}

async function render(bridge, texts) {
    const task = await dash.call(bridge.currentTask);
    if (!task.id) {
        dash.show(dash.message(texts.taskGone));
        return;
    }
    const heading = dash.element("h1", "heading", task.title);
    const subheading = dash.element("p", "subheading", [task.group, task.project].filter(Boolean).join(" · "));
    const items = task.items.filter((item) => item.hasStatus);
    if (items.length === 0) {
        dash.show(heading, subheading, dash.message(texts.noStatusItems));
        return;
    }
    const statuses = await Promise.all(items.map((item) => dash.call(bridge.itemStatus, item.id)));
    const list = dash.element("ul", "list");
    items.forEach((item, index) => list.append(itemRow(bridge, item, statuses[index], texts)));
    dash.show(heading, subheading, list);
}

dash.start(render);
