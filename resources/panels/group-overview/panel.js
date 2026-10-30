// Group overview: the active tasks of the group of the panel's task, by
// priority, with their attention counts. A click opens the task. See
// docs/item-types.md#built-in-panels.
"use strict";

// The CSS colours of the priorities, 0 for Urgent to 3 for Low.
const priorityColours = ["--priorityUrgent", "--priorityHigh", "--priorityNormal", "--priorityLow"];

function taskRow(bridge, task, texts) {
    const row = dash.clickable(dash.element("li", "row"), () => bridge.openTask(task.id));
    row.dataset.id = task.id;
    if (task.attention > 0)
        row.classList.add("attention");

    const line = dash.element("div", "line");
    const marker = dash.element("span", "marker");
    marker.title = task.priorityName;
    marker.style.background = `var(${priorityColours[task.priority] || "--priorityNormal"})`;
    line.append(marker, dash.element("span", "name", task.title));
    if (task.current)
        line.append(dash.element("span", "pill", texts.thisTask));
    if (task.attention > 0) {
        const badge = dash.element("span", "badge", String(task.attention));
        badge.title = task.attentionText;
        line.append(badge);
    }
    row.append(line, dash.element("div", "detail muted", task.project));
    return row;
}

async function render(bridge, texts) {
    const task = await dash.call(bridge.currentTask);
    if (!task.id) {
        dash.show(dash.message(texts.taskGone));
        return;
    }
    const heading = dash.element("h1", "heading", task.group);
    const tasks = await dash.call(bridge.groupTasks, task.groupId);
    if (tasks.length === 0) {
        dash.show(heading, dash.message(texts.noActiveTasks));
        return;
    }
    const list = dash.element("ul", "list");
    for (const each of tasks)
        list.append(taskRow(bridge, each, texts));
    dash.show(heading, list);
}

dash.start(render);
