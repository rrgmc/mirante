// rraidash M1 prototype. See docs/plans/m1-html-prototype.md.
// The whole page is rendered again from `data` and `dev` after every change.

'use strict';

const STORAGE_KEY = 'rraidash-proto-v1';
const PRIORITIES = ['Urgent', 'High', 'Normal', 'Low'];
const STATES = ['active', 'done', 'archived'];

const TYPES = {
  'gitlab.mr.summary': { name: 'GitLab MR (summary)', icon: '⑂' },
  'gitlab.mr.page': { name: 'GitLab MR (page)', icon: '⑂', web: true },
  'gitlab.mr.list': { name: 'GitLab MR List', icon: '☰' },
  'gitlab.pipeline': { name: 'GitLab CI Pipeline', icon: '▶' },
  'gitlab.issue.summary': { name: 'GitLab Issue (summary)', icon: '◉' },
  'gitlab.issue.page': { name: 'GitLab Issue (page)', icon: '◉', web: true },
  'gitlab.issue.list': { name: 'GitLab Issue List', icon: '☰' },
  'jira.issue.summary': { name: 'Jira Issue (summary)', icon: '◆' },
  'jira.issue.page': { name: 'Jira Issue (page)', icon: '◆', web: true },
  'jira.list': { name: 'Jira List', icon: '☰' },
  'claude.session.web': { name: 'Claude session (web)', icon: '✻', web: true },
  'claude.session.local': { name: 'Claude session (local)', icon: '✻' },
  'web.page': { name: 'Web page', icon: '◍', web: true },
  'notes': { name: 'Notes', icon: '✎' },
  'folder': { name: 'Folder / terminal', icon: '▤' },
  'dash.panel': { name: 'Built-in panel', icon: '▦' },
};

const PRESETS = [
  'Full',
  'Two side by side',
  'Two stacked',
  'Grid of four',
  'One large on top, two below',
  'One large on the left, two stacked on the right',
  'Tabs',
];

let data;          // synced model: groups, projects, tasks, rootDashboards
let dev;           // device state, see defaultDev()
let drag = null;   // what is being dragged
let dropTarget = null;
let placing = null; // "Add to dashboard…" waiting for a cell click
let lastTaskClick = { id: null, time: 0 };
let idCounter = 1;

const uid = (prefix) => `${prefix}-${Date.now().toString(36)}${(idCounter++).toString(36)}`;
const $ = (sel) => document.querySelector(sel);

// ---------------------------------------------------------------- state

function defaultDev() {
  return {
    openTabs: data.rootDashboards.map((d) => d.id),
    active: data.rootDashboards[0].id,
    preview: null,
    unlocked: [],
    detached: [],
    focusedCell: null,
    maximised: null,
    expanded: { 'g-work': true, 'g-personal': true, 'p-backend': true },
    filters: { hideLow: false, hideClosed: true },
    sidebarCollapsed: false,
    focusGroup: '',
    attentionFirst: false,
  };
}

function reset() {
  data = JSON.parse(JSON.stringify(window.MOCK));
  forEachDashboard((d) => assignIds(d.layout));
  dev = defaultDev();
}

function load() {
  try {
    const saved = localStorage.getItem(STORAGE_KEY);
    if (saved) {
      const o = JSON.parse(saved);
      data = o.data;
      dev = o.dev;
      return;
    }
  } catch (e) { /* fall through to the mock data */ }
  reset();
}

function save() {
  try { localStorage.setItem(STORAGE_KEY, JSON.stringify({ data, dev })); } catch (e) { /* ignore */ }
}

function commit() {
  save();
  render();
}

// ---------------------------------------------------------------- lookups

const byId = (list, id) => list.find((x) => x.id === id);
const getTask = (id) => byId(data.tasks, id);
const getProject = (id) => byId(data.projects, id);
const getGroup = (id) => byId(data.groups, id);
const getRoot = (id) => byId(data.rootDashboards, id);
const taskDashKey = (taskId, dashId) => `t:${taskId}:${dashId}`;
const ownerTaskOf = (key) => (key.startsWith('t:') ? key.split(':')[1] : null);
const isUnlocked = (key) => dev.unlocked.includes(key);

function getDash(key) {
  const [kind, a, b] = key.split(':');
  if (kind === 'r') return getRoot(a);
  const t = getTask(a);
  return t && byId(t.dashboards, b);
}

function forEachDashboard(fn) {
  data.rootDashboards.forEach(fn);
  data.tasks.forEach((t) => t.dashboards.forEach(fn));
}

function sortByPriority(list) {
  return list.slice().sort((a, b) => (a.priority - b.priority) || (a.order - b.order));
}

function tasksOf(projectId) {
  const all = data.tasks.filter((t) => t.projectId === projectId);
  return all.filter((t) => t.isDefault).concat(sortByPriority(all.filter((t) => !t.isDefault)));
}

const projectsOf = (groupId) => sortByPriority(data.projects.filter((p) => p.groupId === groupId));
const groupsSorted = () => data.groups.slice().sort((a, b) => a.order - b.order);

const itemAttention = (item) => (item.mock && item.mock.attention) || [];
const taskAttention = (t) => (t.state === 'active' ? t.items.flatMap(itemAttention) : []);
const projectAttention = (p) => data.tasks.filter((t) => t.projectId === p.id).flatMap(taskAttention);
const groupAttention = (g) => data.projects.filter((p) => p.groupId === g.id).flatMap(projectAttention);

function whereOf(t) {
  const p = getProject(t.projectId);
  return `${getGroup(p.groupId).name} › ${p.name}`;
}

// Resolves a cell reference. `ownerTaskId` is set inside task dashboards.
function resolve(ref, ownerTaskId) {
  if (!ref) return { kind: 'empty' };
  if (ref.item) {
    const t = getTask(ref.task || ownerTaskId);
    const item = t && byId(t.items, ref.item);
    return item ? { kind: 'item', task: t, item } : { kind: 'removed' };
  }
  if (ref.task) {
    const t = getTask(ref.task);
    const d = t && byId(t.dashboards, ref.dashboard);
    return d ? { kind: 'task', task: t, dash: d } : { kind: 'removed' };
  }
  return { kind: 'empty' };
}

function refTitle(ref, ownerTaskId) {
  const info = resolve(ref, ownerTaskId);
  if (info.kind === 'item') return info.item.name;
  if (info.kind === 'task') return info.task.title;
  return info.kind === 'removed' ? 'Removed' : 'Empty';
}

function titleOf(node, dashKey) {
  return node.type === 'cell' ? refTitle(node.ref, ownerTaskOf(dashKey)) : 'Tab';
}

// ---------------------------------------------------------------- layout trees

function walk(node, fn, parent = null, index = -1) {
  fn(node, parent, index);
  if (node.type === 'split') node.children.forEach((c, i) => walk(c, fn, node, i));
  if (node.type === 'tabs') node.tabs.forEach((t, i) => walk(t.node, fn, node, i));
}

function assignIds(root) {
  walk(root, (n) => { if (!n.id) n.id = uid('n'); });
}

function findNode(root, id) {
  let found = null;
  walk(root, (node, parent, index) => { if (node.id === id) found = { node, parent, index }; });
  return found;
}

const emptyCell = (ref = null) => ({ type: 'cell', id: uid('n'), ref });
const split = (dir, sizes, ...children) => ({ type: 'split', id: uid('n'), dir, sizes, children });

function replaceNode(dash, id, newNode) {
  const f = findNode(dash.layout, id);
  if (!f) return;
  if (!f.parent) dash.layout = newNode;
  else if (f.parent.type === 'split') f.parent.children[f.index] = newNode;
  else f.parent.tabs[f.index].node = newNode;
}

function removeNode(dash, id) {
  const f = findNode(dash.layout, id);
  if (!f) return;
  if (!f.parent) {
    dash.layout = emptyCell();
  } else if (f.parent.type === 'split') {
    f.parent.children.splice(f.index, 1);
    f.parent.sizes.splice(f.index, 1);
  } else {
    f.parent.tabs.splice(f.index, 1);
    if (f.parent.current >= f.parent.tabs.length) f.parent.current = Math.max(0, f.parent.tabs.length - 1);
  }
}

// The normalisation rules in docs/data-model.md#normalisation.
// Rules 5 and 6 need no tree change: cleared and dangling cells render as empty or "Removed".
function normalise(node) {
  if (node.type === 'split') {
    const children = [];
    const sizes = [];
    node.children.forEach((child, i) => {
      const c = normalise(child);
      const size = node.sizes[i] || 1;
      if (c.type === 'split' && c.dir === node.dir) {
        const total = c.sizes.reduce((a, b) => a + b, 0) || 1;
        c.children.forEach((cc, j) => { children.push(cc); sizes.push(size * c.sizes[j] / total); });
      } else {
        children.push(c);
        sizes.push(size);
      }
    });
    if (children.length === 0) return emptyCell();
    if (children.length === 1) return children[0];
    node.children = children;
    node.sizes = sizes;
    return node;
  }
  if (node.type === 'tabs') {
    node.tabs.forEach((t) => { t.node = normalise(t.node); });
    if (node.tabs.length === 0) return emptyCell();
    if (node.tabs.length === 1 && node.collapseSingle) return node.tabs[0].node;
    node.current = Math.min(Math.max(0, node.current), node.tabs.length - 1);
    return node;
  }
  return node;
}

function splitCell(dash, nodeId, side, newNode) {
  const f = findNode(dash.layout, nodeId);
  if (!f) return;
  const dir = side === 'left' || side === 'right' ? 'h' : 'v';
  const first = side === 'left' || side === 'top';
  replaceNode(dash, nodeId, split(dir, [1, 1], ...(first ? [newNode, f.node] : [f.node, newNode])));
}

function addAsTab(dash, dashKey, nodeId, newNode) {
  const f = findNode(dash.layout, nodeId);
  if (!f) return;
  const tab = { title: titleOf(newNode, dashKey), node: newNode };
  if (f.parent && f.parent.type === 'tabs') {
    f.parent.tabs.splice(f.index + 1, 0, tab);
    f.parent.current = f.index + 1;
  } else {
    replaceNode(dash, nodeId, {
      type: 'tabs', id: uid('n'), current: 1, collapseSingle: false,
      tabs: [{ title: titleOf(f.node, dashKey), node: f.node }, tab],
    });
  }
}

function wrapInTabs(dash, dashKey, nodeId) {
  const f = findNode(dash.layout, nodeId);
  if (!f) return;
  replaceNode(dash, nodeId, {
    type: 'tabs', id: uid('n'), current: 0, collapseSingle: false,
    tabs: [{ title: titleOf(f.node, dashKey), node: f.node }],
  });
}

function insertTab(dash, dashKey, tabsId, index, newNode) {
  const f = findNode(dash.layout, tabsId);
  if (!f) return;
  f.node.tabs.splice(index, 0, { title: titleOf(newNode, dashKey), node: newNode });
  f.node.current = index;
}

function makePreset(name, refs, dashKey) {
  let i = 0;
  const c = () => emptyCell(refs[i++] || null);
  switch (name) {
    case 'Two side by side': return split('h', [1, 1], c(), c());
    case 'Two stacked': return split('v', [1, 1], c(), c());
    case 'Grid of four': return split('v', [1, 1], split('h', [1, 1], c(), c()), split('h', [1, 1], c(), c()));
    case 'One large on top, two below': return split('v', [2, 1], c(), split('h', [1, 1], c(), c()));
    case 'One large on the left, two stacked on the right': return split('h', [2, 1], c(), split('v', [1, 1], c(), c()));
    case 'Tabs': {
      const tabs = [];
      for (let n = 0; n < Math.max(1, refs.length); n++) {
        const node = c();
        tabs.push({ title: titleOf(node, dashKey), node });
      }
      return { type: 'tabs', id: uid('n'), current: 0, collapseSingle: false, tabs };
    }
    default: return c();
  }
}

// Applies a layout edit, then normalises and saves.
function edit(dashKey, fn) {
  const dash = getDash(dashKey);
  if (!dash) return;
  fn(dash);
  dash.layout = normalise(dash.layout);
  afterLayoutEdit([dashKey]);
}

function afterLayoutEdit(keys) {
  keys.forEach((k) => { const d = getDash(k); if (d) d.layout = normalise(d.layout); });
  dev.detached = dev.detached.filter((d) => { const dash = getDash(d.dashKey); return dash && findNode(dash.layout, d.nodeId); });
  if (dev.preview && dev.active === 'preview' && keys.includes(taskDashKey(dev.preview.taskId, dev.preview.dashId))) pinPreview();
  commit();
}

// ---------------------------------------------------------------- navigation

function openPreview(taskId, dashId) {
  const t = getTask(taskId);
  if (!t) return;
  dashId = dashId || t.defaultDashboardId;
  // A task that is already pinned as its own tab is shown there.
  const pinned = dev.openTabs.map(getRoot).find((d) => d && d.layout.type === 'cell' && d.layout.ref && d.layout.ref.task === taskId && !d.layout.ref.item);
  if (pinned && pinned.layout.ref.dashboard === dashId) dev.active = pinned.id;
  else {
    dev.preview = { taskId, dashId };
    dev.active = 'preview';
  }
  commit();
}

function pinPreview() {
  const p = dev.preview;
  if (!p) return null;
  const t = getTask(p.taskId);
  const d = { id: uid('r'), name: t.title, order: data.rootDashboards.length, layout: emptyCell({ task: t.id, dashboard: p.dashId }) };
  data.rootDashboards.push(d);
  dev.openTabs.unshift(d.id);
  dev.preview = null;
  dev.active = d.id;
  return d.id;
}

function closePreview() {
  dev.preview = null;
  if (dev.active === 'preview') dev.active = dev.openTabs[0] || 'focus';
  commit();
}

function activateRoot(id) {
  if (!dev.openTabs.includes(id)) dev.openTabs.push(id);
  dev.active = id;
  commit();
}

function closeRootTab(id) {
  const i = dev.openTabs.indexOf(id);
  if (i < 0) return;
  dev.openTabs.splice(i, 1);
  if (dev.active === id) dev.active = dev.openTabs[Math.min(i, dev.openTabs.length - 1)] || (dev.preview ? 'preview' : 'focus');
  commit();
}

function newRootDashboard(name, layout) {
  const d = { id: uid('r'), name: name || `Dashboard ${data.rootDashboards.length + 1}`, order: data.rootDashboards.length, layout: layout || emptyCell() };
  data.rootDashboards.push(d);
  dev.openTabs.push(d.id);
  dev.active = d.id;
  return d;
}

function tabOrder() {
  return (dev.preview ? ['preview'] : []).concat(dev.openTabs);
}

function cycleRootTab(delta) {
  const list = tabOrder();
  if (!list.length) return;
  const i = list.indexOf(dev.active);
  dev.active = list[(i + delta + list.length) % list.length];
  commit();
}

function closeActiveTab() {
  if (dev.active === 'preview') closePreview();
  else if (dev.active !== 'focus') closeRootTab(dev.active);
}

function toggleLock(key) {
  if (!isUnlocked(key)) {
    // Editing the preview pins it.
    if (dev.active === 'preview' && dev.preview && key === taskDashKey(dev.preview.taskId, dev.preview.dashId)) pinPreview();
    dev.unlocked.push(key);
  } else {
    dev.unlocked = dev.unlocked.filter((k) => k !== key);
  }
  commit();
}

function setFocus(placement) {
  dev.focusedCell = placement;
  applyFocus();
  save();
}

function startPlacing(ref, rootId, label) {
  activateRoot(rootId);
  placing = { ref, dashKey: `r:${rootId}`, label };
  render();
}

function place(node) {
  const p = placing;
  placing = null;
  edit(p.dashKey, (dash) => {
    if (node.ref === null) node.ref = p.ref;
    else splitCell(dash, node.id, 'right', emptyCell(p.ref));
  });
}

// ---------------------------------------------------------------- DOM helpers

function h(spec, attrs, ...kids) {
  const [tag, ...classes] = spec.split('.');
  const el = document.createElement(tag || 'div');
  if (classes.length) el.className = classes.filter(Boolean).join(' ');
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v == null || v === false) continue;
    if (k.startsWith('on')) el.addEventListener(k.slice(2), v);
    else if (k === 'class') el.className = `${el.className} ${v}`.trim();
    else if (k === 'style') el.style.cssText = v;
    else if (typeof v === 'boolean') el[k] = v;
    else el.setAttribute(k, v);
  }
  for (const kid of kids.flat(Infinity)) {
    if (kid == null || kid === false) continue;
    el.append(kid instanceof Node ? kid : String(kid));
  }
  return el;
}

function iconBtn(label, title, action, on) {
  return h('button.icon-btn', {
    title, class: on ? 'on' : null,
    onclick: (e) => { e.stopPropagation(); action(e); },
    onmousedown: (e) => e.stopPropagation(),
  }, label);
}

const marker = (priority) => h('span.marker', { class: `p${priority}`, title: PRIORITIES[priority] });

function badge(list) {
  return list.length ? h('span.badge', { title: list.join('\n') }, list.length) : null;
}

let toastTimer;
function toast(msg) {
  const el = $('#toast');
  el.textContent = msg;
  el.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove('show'), 2200);
}

// ---------------------------------------------------------------- render

function render() {
  const tree = document.querySelector('.sidebar-tree');
  const scroll = tree ? tree.scrollTop : 0;
  $('#app').replaceChildren(renderSidebar(), renderMain());
  const newTree = document.querySelector('.sidebar-tree');
  if (newTree) newTree.scrollTop = scroll;
  renderFloating();
  applyFocus();
  applyMaximised();
}

// --- sidebar

function renderSidebar() {
  if (dev.sidebarCollapsed) {
    return h('aside.sidebar.collapsed', {},
      h('div.sidebar-head', {}, iconBtn('»', 'Show sidebar', () => { dev.sidebarCollapsed = false; commit(); })));
  }
  const f = dev.filters;
  const rows = [];
  const total = data.groups.flatMap(groupAttention);
  rows.push(h('div.row-entry.focus-entry', {
    class: dev.active === 'focus' ? 'selected' : null,
    onclick: () => { dev.active = 'focus'; commit(); },
  }, h('span.twisty', {}, '◎'), h('span.label', {}, 'Focus'), badge(total)));

  for (const g of groupsSorted()) {
    const open = !!dev.expanded[g.id];
    rows.push(h('div.row-entry.group', {
      'data-id': g.id,
      onclick: () => { dev.expanded[g.id] = !open; commit(); },
      oncontextmenu: (e) => { e.preventDefault(); groupMenu(e, g); },
    }, h('span.twisty', {}, open ? '▾' : '▸'), h('span.label', {}, g.name), badge(groupAttention(g))));
    if (!open) continue;
    for (const p of projectsOf(g.id)) {
      if (f.hideLow && p.priority === 3) continue;
      rows.push(projectRow(p));
      if (!dev.expanded[p.id]) continue;
      for (const t of tasksOf(p.id)) {
        if (f.hideLow && t.priority === 3 && !t.isDefault) continue;
        if (f.hideClosed && t.state !== 'active') continue;
        rows.push(taskRow(t));
        if (!dev.expanded[t.id]) continue;
        for (const item of t.items) rows.push(itemRow(t, item));
      }
    }
  }

  return h('aside.sidebar', {},
    h('div.sidebar-head', {},
      h('span.title', {}, 'rraidash'),
      iconBtn('⌕', 'Quick switcher (Ctrl+K)', openSwitcher),
      iconBtn('«', 'Hide sidebar', () => { dev.sidebarCollapsed = true; commit(); })),
    h('div.sidebar-tree', {}, rows),
    h('div.sidebar-foot', {},
      h('label', {}, h('input', { type: 'checkbox', checked: f.hideLow, onchange: (e) => { f.hideLow = e.target.checked; commit(); } }), 'Hide Low'),
      h('label', {}, h('input', { type: 'checkbox', checked: f.hideClosed, onchange: (e) => { f.hideClosed = e.target.checked; commit(); } }), 'Hide Done and Archived'),
      h('div.row', {},
        h('button.btn', { onclick: openHelp }, 'Shortcuts'),
        h('button.btn', { onclick: () => { if (confirm('Discard all changes and restore the mock data?')) { reset(); commit(); } } }, 'Reset'))));
}

function projectRow(p) {
  const open = !!dev.expanded[p.id];
  const row = h('div.row-entry', {
    'data-id': p.id,
    class: p.priority === 3 ? 'dim' : null,
    style: 'padding-left: 20px',
    draggable: 'true',
    onclick: () => { dev.expanded[p.id] = !open; commit(); },
    oncontextmenu: (e) => { e.preventDefault(); projectMenu(e, p); },
    ondragstart: (e) => { drag = { kind: 'project', projectId: p.id }; e.dataTransfer.setData('text/plain', p.name); },
  }, h('span.twisty', {}, open ? '▾' : '▸'), marker(p.priority), h('span.label', {}, p.name), badge(projectAttention(p)));
  sidebarDropTarget(row, () => drag && drag.kind === 'project' && drag.projectId !== p.id && getProject(drag.projectId).groupId === p.groupId,
    (after) => {
      const moved = getProject(drag.projectId);
      reorder(data.projects.filter((x) => x.groupId === p.groupId), moved, p, after);
    });
  return row;
}

function taskRow(t) {
  const open = !!dev.expanded[t.id];
  const selected = dev.active === 'preview' && dev.preview && dev.preview.taskId === t.id;
  const row = h('div.row-entry', {
    'data-id': t.id,
    class: [selected ? 'selected' : '', t.priority === 3 && !t.isDefault ? 'dim' : '', t.state !== 'active' ? t.state : ''].join(' ').trim() || null,
    style: 'padding-left: 34px',
    draggable: 'true',
    onclick: () => onTaskClick(t),
    oncontextmenu: (e) => { e.preventDefault(); taskMenu(e, t); },
    ondragstart: (e) => { drag = { kind: 'task', taskId: t.id }; e.dataTransfer.setData('text/plain', t.title); },
  },
  h('span.twisty', { onclick: (e) => { e.stopPropagation(); dev.expanded[t.id] = !open; commit(); } }, t.items.length ? (open ? '▾' : '▸') : ''),
  t.isDefault ? h('span.marker') : marker(t.priority),
  h('span.label', {}, t.title),
  t.state !== 'active' ? h('span.state-tag', {}, t.state) : null,
  badge(taskAttention(t)));
  if (!t.isDefault) {
    sidebarDropTarget(row, () => drag && drag.kind === 'task' && drag.taskId !== t.id && getTask(drag.taskId).projectId === t.projectId && !getTask(drag.taskId).isDefault,
      (after) => {
        const moved = getTask(drag.taskId);
        reorder(data.tasks.filter((x) => x.projectId === t.projectId && !x.isDefault), moved, t, after);
      });
  }
  return row;
}

function itemRow(t, item) {
  return h('div.row-entry', {
    'data-id': item.id,
    style: 'padding-left: 52px',
    draggable: 'true',
    title: TYPES[item.type].name,
    onclick: () => openPreview(t.id),
    oncontextmenu: (e) => { e.preventDefault(); itemMenu(e, t, item); },
    ondragstart: (e) => { drag = { kind: 'item', taskId: t.id, itemId: item.id }; e.dataTransfer.setData('text/plain', item.name); },
  }, h('span.type-icon', {}, TYPES[item.type].icon), h('span.label', {}, item.name), badge(itemAttention(item)));
}

function onTaskClick(t) {
  const now = Date.now();
  const double = lastTaskClick.id === t.id && now - lastTaskClick.time < 450;
  lastTaskClick = { id: t.id, time: now };
  if (double && dev.active === 'preview' && dev.preview && dev.preview.taskId === t.id) {
    pinPreview();
    commit();
  } else {
    openPreview(t.id);
  }
}

// Makes a sidebar row accept reorder drops. `accepts` checks the drag; `apply(after)` reorders.
function sidebarDropTarget(row, accepts, apply) {
  row.addEventListener('dragover', (e) => {
    if (!accepts()) return;
    e.preventDefault();
    const r = row.getBoundingClientRect();
    const after = e.clientY > r.top + r.height / 2;
    row.classList.toggle('drop-before', !after);
    row.classList.toggle('drop-after', after);
  });
  row.addEventListener('dragleave', () => row.classList.remove('drop-before', 'drop-after'));
  row.addEventListener('drop', (e) => {
    if (!accepts()) return;
    e.preventDefault();
    const after = row.classList.contains('drop-after');
    apply(after);
    drag = null;
    commit();
  });
}

// Moves `moved` next to `target`. Dropping next to an entry of another level takes that level.
function reorder(entities, moved, target, after) {
  const old = moved.priority;
  const list = sortByPriority(entities.filter((e) => e !== moved));
  moved.priority = target.priority;
  list.splice(list.indexOf(target) + (after ? 1 : 0), 0, moved);
  list.forEach((e, n) => { e.order = n; });
  if (old !== moved.priority) toast(`Priority changed to ${PRIORITIES[moved.priority]}`);
}

// --- main area

function renderMain() {
  const main = h('div.main', {}, renderTabBar());
  if (placing) {
    main.append(h('div.banner', {}, `Click a cell to place “${placing.label}”. An empty cell is filled; any other cell is split.`,
      h('button.btn', { onclick: () => { placing = null; render(); } }, 'Cancel')));
  }
  if (dev.active === 'focus') {
    main.append(renderFocusToolbar(), h('div.content', {}, renderFocus()));
  } else if (dev.active === 'preview' && dev.preview && getTask(dev.preview.taskId)) {
    const t = getTask(dev.preview.taskId);
    const d = byId(t.dashboards, dev.preview.dashId) || byId(t.dashboards, t.defaultDashboardId);
    const key = taskDashKey(t.id, d.id);
    main.append(h('div.toolbar', {},
      h('span.crumbs', {}, whereOf(t), ' ›'),
      marker(t.priority),
      h('span.name', { style: 'font-style: italic' }, t.title),
      dashboardPicker(t, d.id, (id) => { dev.preview.dashId = id; commit(); }),
      h('span.spacer'),
      h('button.btn', { onclick: (e) => presetMenu(e, key) }, 'Apply preset'),
      lockBtn(key),
      h('button.btn', { title: 'Keep this task open as a tab', onclick: () => { pinPreview(); commit(); } }, '📌 Pin')));
    main.append(h('div.content', {}, renderNode(d.layout, { dashKey: key })));
  } else {
    const d = getRoot(dev.active);
    if (!d) {
      main.append(h('div.content', {}, h('div.focus-view', { style: 'padding: 24px' }, 'No tab is open. Click a task in the sidebar, open Focus, or press + for a new root dashboard.')));
    } else {
      const key = `r:${d.id}`;
      main.append(h('div.toolbar', {},
        h('span.name', {}, d.name),
        h('span.crumbs', {}, 'root dashboard'),
        h('span.spacer'),
        h('button.btn', { onclick: (e) => presetMenu(e, key) }, 'Apply preset'),
        lockBtn(key)));
      main.append(h('div.content', {}, renderNode(d.layout, { dashKey: key })));
    }
  }
  return main;
}

function lockBtn(key) {
  const on = isUnlocked(key);
  return h('button.btn', {
    class: on ? 'on' : null,
    title: on ? 'Editing is on. Click to lock the layout.' : 'The layout is locked. Click to edit it.',
    onclick: (e) => { e.stopPropagation(); toggleLock(key); },
    onmousedown: (e) => e.stopPropagation(),
  }, on ? '🔓 Editing' : '🔒 Locked');
}

function dashboardPicker(t, currentId, onPick) {
  return h('select', {
    title: 'Task dashboard',
    onmousedown: (e) => e.stopPropagation(),
    onclick: (e) => e.stopPropagation(),
    onchange: (e) => {
      if (e.target.value === '__new') { newTaskDashboard(t, onPick); render(); return; }
      onPick(e.target.value);
    },
  },
  t.dashboards.map((d) => h('option', { value: d.id, selected: d.id === currentId }, d.name)),
  h('option', { value: '__new' }, 'New dashboard…'));
}

function newTaskDashboard(t, onCreated) {
  const name = prompt('Name of the new dashboard', `Dashboard ${t.dashboards.length + 1}`);
  if (!name) return;
  openPicker({
    placeholder: 'Choose a starting layout',
    entries: PRESETS.map((p) => ({ kind: 'Preset', label: p, action: () => {
      const d = { id: uid('d'), name, layout: null };
      d.layout = makePreset(p, t.items.map((i) => ({ item: i.id })), taskDashKey(t.id, d.id));
      t.dashboards.push(d);
      onCreated(d.id);
    } })),
  });
}

function presetMenu(e, key) {
  const owner = ownerTaskOf(key);
  const refs = owner ? getTask(owner).items.map((i) => ({ item: i.id })) : [];
  showMenu(e.clientX, e.clientY, [
    { head: owner ? 'Fills cells with the task items' : 'Creates empty cells' },
    ...PRESETS.map((p) => ({ label: p, action: () => {
      if (!confirm('Replace the current layout?')) return;
      edit(key, (dash) => { dash.layout = makePreset(p, refs, key); });
    } })),
  ]);
}

function renderTabBar() {
  const tabs = [];
  if (dev.preview) {
    const t = getTask(dev.preview.taskId);
    if (t) {
      tabs.push(h('div.root-tab.preview', {
        class: dev.active === 'preview' ? 'active' : null,
        title: `Preview of “${t.title}”. The next click in the sidebar replaces it. Double-click to keep it.`,
        onclick: () => { dev.active = 'preview'; commit(); },
        ondblclick: () => { pinPreview(); commit(); },
      }, h('span.label', {}, t.title),
      iconBtn('📌', 'Pin', () => { pinPreview(); commit(); }),
      iconBtn('×', 'Close (Ctrl+W)', closePreview)));
    }
  }
  for (const id of dev.openTabs) {
    const d = getRoot(id);
    if (!d) continue;
    tabs.push(h('div.root-tab', {
      class: dev.active === id ? 'active' : null,
      onclick: () => { dev.active = id; commit(); },
      oncontextmenu: (e) => { e.preventDefault(); rootTabMenu(e, d); },
    }, h('span.label', {}, d.name), iconBtn('×', 'Close tab', () => closeRootTab(id))));
  }
  tabs.push(h('span.add', {}, iconBtn('+', 'New root dashboard', () => {
    const d = newRootDashboard();
    dev.unlocked.push(`r:${d.id}`);
    commit();
  })));
  return h('div.root-tabs', {}, tabs);
}

// --- layout engine

function renderNode(node, ctx) {
  if (node.type === 'split') return renderSplit(node, ctx);
  if (node.type === 'tabs') return renderTabs(node, ctx);
  return renderCell(node, ctx);
}

function renderSplit(node, ctx) {
  const el = h(`div.split.${node.dir}`);
  node.children.forEach((child, i) => {
    if (i > 0) el.append(h('div.splitter', { onmousedown: (e) => startResize(e, node, i) }));
    el.append(h('div.pane', { style: `flex-grow: ${node.sizes[i]}` }, renderNode(child, ctx)));
  });
  return el;
}

// Resizing works whether or not the layout is locked.
function startResize(e, node, i) {
  e.preventDefault();
  e.stopPropagation();
  const bar = e.currentTarget;
  const a = bar.previousElementSibling;
  const b = bar.nextElementSibling;
  const horiz = node.dir === 'h';
  const ra = a.getBoundingClientRect();
  const rb = b.getBoundingClientRect();
  const pxA = horiz ? ra.width : ra.height;
  const pxAll = pxA + (horiz ? rb.width : rb.height);
  const sum = node.sizes[i - 1] + node.sizes[i];
  const start = horiz ? e.clientX : e.clientY;
  bar.classList.add('dragging');
  const move = (ev) => {
    const px = Math.max(40, Math.min(pxAll - 40, pxA + (horiz ? ev.clientX : ev.clientY) - start));
    node.sizes[i - 1] = sum * px / pxAll;
    node.sizes[i] = sum - node.sizes[i - 1];
    a.style.flexGrow = node.sizes[i - 1];
    b.style.flexGrow = node.sizes[i];
  };
  const up = () => {
    document.removeEventListener('mousemove', move);
    document.removeEventListener('mouseup', up);
    bar.classList.remove('dragging');
    save();
  };
  document.addEventListener('mousemove', move);
  document.addEventListener('mouseup', up);
}

function renderTabs(node, ctx) {
  const bar = h('div.tabbar', { 'data-dash': ctx.dashKey, 'data-node': node.id });
  node.tabs.forEach((t, i) => {
    bar.append(h('div.tab', {
      class: i === node.current ? 'current' : null,
      'data-index': i,
      onclick: () => { node.current = i; commit(); },
      oncontextmenu: (e) => { e.preventDefault(); e.stopPropagation(); tabMenu(e, ctx.dashKey, node, i); },
    }, t.title));
  });
  bar.append(h('div.fill'));
  const current = node.tabs[node.current];
  return h('div.tabs', { 'data-dash': ctx.dashKey, 'data-node': node.id },
    bar, h('div.tabbody', {}, current ? renderNode(current.node, ctx) : null));
}

function renderCell(node, ctx) {
  const placement = `${ctx.dashKey}/${node.id}`;
  const editable = isUnlocked(ctx.dashKey);
  const attrs = {
    'data-dash': ctx.dashKey,
    'data-node': node.id,
    'data-placement': placement,
    class: [editable ? 'editable' : '', placing && placing.dashKey === ctx.dashKey ? 'placing' : ''].join(' ').trim() || null,
    onmousedown: (e) => { if (!e.focusDone) { e.focusDone = true; setFocus(placement); } },
    oncontextmenu: (e) => { e.preventDefault(); e.stopPropagation(); cellMenu(e, ctx, node); },
  };
  if (!ctx.floating && dev.detached.some((d) => d.dashKey === ctx.dashKey && d.nodeId === node.id)) {
    return h('div.cell.detached-ph', { ...attrs, onclick: () => attach(ctx.dashKey, node.id) },
      h('div.cell-body', {}, 'Detached. Click to bring back.'));
  }
  const info = resolve(node.ref, ownerTaskOf(ctx.dashKey));
  let el;
  if (info.kind === 'task') {
    el = renderTaskCell(node, ctx, info, attrs);
  } else {
    if (info.kind === 'item') attrs['data-item'] = info.item.id;
    el = h('div.cell', attrs, cellHead(node, ctx, info), h('div.cell-body', {}, cellBody(node, ctx, info)));
    el.classList.add(info.kind === 'item' ? 'item-cell' : info.kind);
  }
  if (placing && placing.dashKey === ctx.dashKey) {
    el.addEventListener('click', (e) => { e.stopPropagation(); e.preventDefault(); place(node); }, true);
  }
  return el;
}

function headDrag(node, ctx, title) {
  if (ctx.floating || !isUnlocked(ctx.dashKey)) return {};
  return {
    draggable: 'true',
    ondragstart: (e) => {
      e.stopPropagation();
      drag = { kind: 'cell', dashKey: ctx.dashKey, nodeId: node.id, ref: node.ref };
      e.dataTransfer.setData('text/plain', title);
      e.dataTransfer.effectAllowed = 'move';
    },
  };
}

function cellHead(node, ctx, info) {
  const menuBtn = iconBtn('⋯', 'Menu', (e) => cellMenu(e, ctx, node));
  if (info.kind !== 'item') {
    return h('div.cell-head', headDrag(node, ctx, 'Empty'),
      h('span.label.muted', {}, info.kind === 'removed' ? 'Removed' : 'Empty cell'), menuBtn);
  }
  const { item } = info;
  const type = TYPES[item.type];
  const inRoot = !ownerTaskOf(ctx.dashKey);
  return h('div.cell-head', { ...headDrag(node, ctx, item.name), title: `${type.name}${inRoot ? ` · from “${info.task.title}”` : ''}` },
    h('span.type-icon', {}, type.icon),
    h('span.label', {}, item.name, inRoot ? h('span.muted', {}, `  · ${info.task.title}`) : null),
    badge(itemAttention(item)),
    iconBtn('⟳', 'Refresh', () => toast(`Refreshed “${item.name}”`)),
    iconBtn('↗', 'Open in browser', () => toast(`Would open ${itemUrl(item, info.task)} in the browser`)),
    ctx.floating ? null : iconBtn('⧉', 'Detach', () => detach(ctx.dashKey, node.id)),
    menuBtn);
}

function cellBody(node, ctx, info) {
  if (info.kind === 'removed') return 'Removed. The item, task or dashboard this cell showed no longer exists.';
  if (info.kind === 'empty') {
    if (!isUnlocked(ctx.dashKey)) return 'Empty cell. Turn on editing to fill it.';
    return h('div', {}, h('div', {}, 'Drag an item here from the sidebar,'), h('div', {}, 'or ', h('button.btn', { onclick: () => changeRef(ctx, node) }, 'choose one…')));
  }
  return itemBody(info.item, info.task);
}

function renderTaskCell(node, ctx, info, attrs) {
  const { task: t, dash: d } = info;
  const key = taskDashKey(t.id, d.id);
  return h('div.cell.task-cell', attrs,
    h('div.cell-head', { ...headDrag(node, ctx, t.title), title: whereOf(t) },
      marker(t.priority),
      h('span.label', {}, t.title, h('span.muted', {}, `  · ${whereOf(t)}`)),
      badge(taskAttention(t)),
      dashboardPicker(t, d.id, (id) => { node.ref.dashboard = id; commit(); }),
      iconBtn(isUnlocked(key) ? '🔓' : '🔒', isUnlocked(key) ? 'Editing this task dashboard. Click to lock it.' : 'This task dashboard is locked. Click to edit it.', () => toggleLock(key), isUnlocked(key)),
      iconBtn('↗', 'Open the task alone', () => openPreview(t.id, d.id)),
      ctx.floating ? null : iconBtn('⧉', 'Detach', () => detach(ctx.dashKey, node.id)),
      iconBtn('⋯', 'Menu', (e) => cellMenu(e, ctx, node))),
    h('div.cell-body', {}, renderNode(d.layout, { dashKey: key, floating: ctx.floating })));
}

// --- mock panel contents

function itemUrl(item, t) {
  const c = item.config || {};
  if (c.url) return c.url;
  const p = getProject(t.projectId);
  if (item.type === 'gitlab.mr.summary') return `https://gitlab.com/${c.project}/-/merge_requests/${c.iid}`;
  if (item.type === 'jira.issue.summary') return `https://acme.atlassian.net/browse/${c.key}`;
  return `https://gitlab.com/${p.gitlabProject || ''}`;
}

const pill = (state) => h('span.pill', { class: state || 'neutral' }, state || 'none');
const kv = (pairs) => h('div.kv', {}, pairs.flatMap(([k, v]) => [h('span.k', {}, k), h('span', {}, v)]));
const actionBtn = (label) => h('button.btn', { onclick: () => toast(`${label}: done (mock)`) }, label);
const actions = (...labels) => h('div.mt', { style: 'display: flex; gap: 6px; flex-wrap: wrap' }, labels.map(actionBtn));

function mockTable(headers, rows) {
  return h('table.mock', {}, h('tr', {}, headers.map((x) => h('th', {}, x))), rows.map((r) => h('tr', {}, r.map((x) => h('td', {}, x)))));
}

function itemBody(item, t) {
  const c = item.config || {};
  const m = item.mock || {};
  const p = getProject(t.projectId);
  const g = getGroup(p.groupId);
  if (item.type === 'dash.panel') return groupOverview(getGroup(c.groupId) || g);
  if (TYPES[item.type].web) {
    return h('div.web-ph', {},
      h('div', {}, `${TYPES[item.type].name}, shown as a placeholder`),
      h('div.url', {}, itemUrl(item, t)),
      h('div', {}, `Login profile: ${g.name}`));
  }
  switch (item.type) {
    case 'gitlab.mr.summary':
      return h('div', {},
        h('h4', {}, item.name),
        kv([['State', m.state || 'open'], ['Branches', `${c.project ? 'feature' : 'branch'}/${c.iid} → main`], ['Author', 'you'],
          ['Reviewers', 'dana, li'], ['Pipeline', pill(m.pipeline)], ['Approvals', m.attention && m.attention.includes('Approved') ? '2 of 2 (dana, li)' : '0 of 2']]),
        m.pipeline === 'failed' ? h('div.mt', {}, 'Failed jobs: ', h('b', {}, 'test:unit'), ', ', h('b', {}, 'lint')) : null,
        actions('Approve', 'Retry pipeline', 'Comment'));
    case 'gitlab.mr.list':
      return mockTable(['Ref', 'Title', 'Author', 'Pipeline', 'Approvals', 'Updated'], [
        ['!233', 'Cache user lookups', 'dana', pill('passed'), '1/2', '2 h ago'],
        ['!231', 'Fix session refresh', 'you', pill('failed'), '0/2', '3 h ago'],
        ['!228', 'Token bucket limiter', 'you', pill('passed'), '2/2', 'yesterday'],
        ['!226', 'Bump dependencies', 'renovate', pill('running'), '0/1', 'yesterday'],
      ]);
    case 'gitlab.issue.list':
      return mockTable(['Ref', 'Title', 'Assignee', 'Labels', 'Updated'], [
        ['#87', 'Dark mode', 'you', 'feature', 'today'],
        ['#85', 'Date picker overflows on mobile', 'li', 'bug', '2 days ago'],
        ['#80', 'Upgrade to React 19', '', 'chore', 'last week'],
      ]);
    case 'jira.list':
      return mockTable(['Key', 'Summary', 'Status', 'Assignee', 'Updated'], [
        ['ACME-412', 'Login times out after 30 s', 'In Progress', 'you', 'today'],
        ['ACME-398', 'Rate limit the public API', 'In Review', 'you', 'today'],
        ['ACME-405', 'Export audit log as CSV', 'To Do', 'dana', 'yesterday'],
        ['ACME-371', 'Remove legacy v1 endpoints', 'To Do', '', 'last week'],
      ]);
    case 'gitlab.pipeline': {
      const failed = m.pipeline === 'failed';
      const job = (name, state) => h('div', {}, pill(state), ' ', name);
      return h('div', {},
        kv([['Branch', c.branch || 'main'], ['Status', pill(m.pipeline)], ['Duration', '6 min 12 s']]),
        h('div.stages.mt', {},
          h('div.stage', {}, h('span.sname', {}, 'build'), job('compile', 'passed')),
          h('div.stage', {}, h('span.sname', {}, 'test'), job('test:unit', failed ? 'failed' : 'passed'), job('lint', failed ? 'failed' : 'passed')),
          h('div.stage', {}, h('span.sname', {}, 'deploy'), job('staging', failed ? null : (m.pipeline === 'running' ? 'running' : 'passed')))),
        actions('Retry', 'Cancel'));
    }
    case 'gitlab.issue.summary':
      return h('div', {}, h('h4', {}, item.name),
        kv([['State', 'open'], ['Assignees', 'you'], ['Labels', 'feature, ui'], ['Milestone', '2026.11'], ['Due', '20 Nov 2026']]),
        h('div.mt.muted', {}, 'Latest comment, li: “Can we follow the system setting by default?”'),
        actions('Comment', 'Close'));
    case 'jira.issue.summary':
      return h('div', {}, h('h4', {}, `${c.key}: ${m.summary || item.name}`),
        kv([['Status', h('select', {
          onchange: (e) => { m.status = e.target.value; item.mock = m; toast(`Status changed to ${m.status} (mock)`); save(); },
          onmousedown: (e) => e.stopPropagation(),
        }, ['To Do', 'In Progress', 'In Review', 'Done'].map((s) => h('option', { selected: s === m.status }, s)))],
        ['Type', 'Story'], ['Assignee', 'you'], ['Reporter', 'dana'], ['Sprint', 'Sprint 44']]),
        h('div.mt', {}, 'Description: steps to reproduce, expected and actual results, rendered from Jira.'),
        actions('Comment'));
    case 'claude.session.local':
      return h('div', {},
        kv([['Session', c.sessionId], ['Folder', `${p.name} (main folder)`], ['Device', c.device]]),
        h('div.mt', {}, h('button.btn', { onclick: () => toast(`Would run: claude --resume ${c.sessionId}`) }, 'Resume in terminal')));
    case 'notes':
      return h('textarea.notes-area', {
        placeholder: 'Markdown notes…',
        oninput: (e) => { c.text = e.target.value; item.config = c; save(); },
        onmousedown: (e) => e.stopPropagation(),
      }, c.text || '');
    case 'folder':
      return h('div', {},
        kv([['Path', `D:\\src\\${c.folder}`], ['Branch', 'fix/login-timeout'], ['Status', '2 modified, 1 untracked']]),
        actions('Terminal', 'Editor', 'File manager'));
    default:
      return TYPES[item.type].name;
  }
}

function groupOverview(g) {
  const ts = sortByPriority(data.tasks.filter((t) => t.state === 'active' && !t.isDefault && getProject(t.projectId).groupId === g.id));
  return h('div', {}, h('h4', {}, `${g.name}: active tasks`),
    ts.map((t) => h('div.focus-row', { onclick: () => openPreview(t.id) },
      marker(t.priority), h('span.title', {}, t.title), h('span.where', {}, getProject(t.projectId).name), badge(taskAttention(t)))));
}

// --- Focus view

function focusTasks() {
  let ts = data.tasks.filter((t) => t.state === 'active' && !t.isDefault);
  if (dev.focusGroup) ts = ts.filter((t) => getProject(t.projectId).groupId === dev.focusGroup);
  const att = (t) => (taskAttention(t).length ? 1 : 0);
  return ts.sort((a, b) =>
    (dev.attentionFirst ? att(b) - att(a) : 0)
    || a.priority - b.priority
    || getProject(a.projectId).priority - getProject(b.projectId).priority
    || a.order - b.order);
}

function renderFocusToolbar() {
  return h('div.toolbar', {},
    h('span.name', {}, 'Focus'),
    h('select', { onchange: (e) => { dev.focusGroup = e.target.value; commit(); } },
      h('option', { value: '' }, 'All groups'),
      groupsSorted().map((g) => h('option', { value: g.id, selected: dev.focusGroup === g.id }, g.name))),
    h('label', {}, h('input', { type: 'checkbox', checked: dev.attentionFirst, onchange: (e) => { dev.attentionFirst = e.target.checked; commit(); } }), ' Needs attention first'));
}

function renderFocus() {
  const rows = focusTasks().map((t) => h('div.focus-row', {
    class: t.priority === 3 ? 'dim' : null,
    style: t.priority === 3 ? 'opacity: 0.55' : null,
    onclick: () => openPreview(t.id),
  }, marker(t.priority), h('span.title', {}, t.title), h('span.where', {}, whereOf(t)), badge(taskAttention(t))));
  return h('div.focus-view', {}, rows.length ? rows : h('div', { style: 'padding: 24px' }, 'No active tasks.'));
}

// --- detached windows

function detach(dashKey, nodeId) {
  const n = dev.detached.length;
  dev.detached.push({ dashKey, nodeId, x: 160 + 30 * n, y: 120 + 30 * n, w: 560, h: 400 });
  commit();
}

function attach(dashKey, nodeId) {
  dev.detached = dev.detached.filter((d) => !(d.dashKey === dashKey && d.nodeId === nodeId));
  commit();
}

function renderFloating() {
  const layer = $('#floating');
  layer.replaceChildren();
  for (const w of dev.detached) {
    const dash = getDash(w.dashKey);
    const f = dash && findNode(dash.layout, w.nodeId);
    if (!f) continue;
    const title = h('div.float-title', {}, h('span.label', {}, `${titleOf(f.node, w.dashKey)} (detached window)`),
      iconBtn('✕', 'Close the window and put the panel back', () => attach(w.dashKey, w.nodeId)));
    const win = h('div.float-win', { style: `left: ${w.x}px; top: ${w.y}px; width: ${w.w}px; height: ${w.h}px` },
      title, renderNode(f.node, { dashKey: w.dashKey, floating: true }));
    title.addEventListener('mousedown', (e) => {
      const sx = e.clientX - w.x;
      const sy = e.clientY - w.y;
      const move = (ev) => { w.x = ev.clientX - sx; w.y = Math.max(0, ev.clientY - sy); win.style.left = `${w.x}px`; win.style.top = `${w.y}px`; };
      const up = () => { document.removeEventListener('mousemove', move); document.removeEventListener('mouseup', up); save(); };
      document.addEventListener('mousemove', move);
      document.addEventListener('mouseup', up);
    });
    win.addEventListener('mouseup', () => { w.w = win.offsetWidth; w.h = win.offsetHeight; save(); });
    layer.append(win);
  }
}

// --- focus and maximise

function applyFocus() {
  document.querySelectorAll('.cell.focused').forEach((el) => el.classList.remove('focused'));
  if (!dev.focusedCell) return;
  const el = document.querySelector(`.cell[data-placement="${CSS.escape(dev.focusedCell)}"]`);
  if (el) el.classList.add('focused');
}

function applyMaximised() {
  if (!dev.maximised) return;
  const content = document.querySelector('.main .content');
  const el = content && content.querySelector(`.cell[data-placement="${CSS.escape(dev.maximised)}"]`);
  if (!el) return;
  const r = content.getBoundingClientRect();
  el.classList.add('maximised');
  Object.assign(el.style, { left: `${r.left + 4}px`, top: `${r.top + 4}px`, width: `${r.width - 8}px`, height: `${r.height - 8}px` });
}

function toggleMaximise() {
  if (dev.maximised) dev.maximised = null;
  else if (dev.focusedCell) dev.maximised = dev.focusedCell;
  else return toast('Click a cell first, then maximise it');
  commit();
}

// Innermost visible cells, so focus moves between panels rather than task cells.
function visibleCells() {
  return [...document.querySelectorAll('.main .content .cell[data-placement]')]
    .filter((el) => !el.querySelector('.cell') && el.getBoundingClientRect().width > 0);
}

function moveFocus(dir) {
  const cells = visibleCells();
  if (!cells.length) return;
  const cur = cells.find((el) => el.dataset.placement === dev.focusedCell);
  if (!cur) { setFocus(cells[0].dataset.placement); return; }
  const a = cur.getBoundingClientRect();
  const ax = a.left + a.width / 2;
  const ay = a.top + a.height / 2;
  let best = null;
  let bestScore = Infinity;
  for (const el of cells) {
    if (el === cur) continue;
    const b = el.getBoundingClientRect();
    const bx = b.left + b.width / 2;
    const by = b.top + b.height / 2;
    const ok = { left: b.right <= a.left + 2, right: b.left >= a.right - 2, up: b.bottom <= a.top + 2, down: b.top >= a.bottom - 2 }[dir];
    if (!ok) continue;
    const primary = dir === 'left' || dir === 'right' ? Math.abs(bx - ax) : Math.abs(by - ay);
    const secondary = dir === 'left' || dir === 'right' ? Math.abs(by - ay) : Math.abs(bx - ax);
    const score = primary + 2 * secondary;
    if (score < bestScore) { bestScore = score; best = el; }
  }
  if (best) setFocus(best.dataset.placement);
}

function cycleInnerTab(delta) {
  const cell = dev.focusedCell && document.querySelector(`.cell[data-placement="${CSS.escape(dev.focusedCell)}"]`);
  const tabsEl = cell && cell.closest('.tabs');
  if (!tabsEl) return toast('The focused cell is not in a tab group');
  const dash = getDash(tabsEl.dataset.dash);
  const f = dash && findNode(dash.layout, tabsEl.dataset.node);
  if (!f) return;
  f.node.current = (f.node.current + delta + f.node.tabs.length) % f.node.tabs.length;
  commit();
  const first = document.querySelector(`.tabs[data-node="${CSS.escape(f.node.id)}"] .cell[data-placement]`);
  if (first) setFocus(first.dataset.placement);
}

// ---------------------------------------------------------------- menus

function closeMenu() {
  $('#menu-layer').replaceChildren();
}

function showMenu(x, y, entries) {
  closeMenu();
  const menu = buildMenu(entries);
  $('#menu-layer').append(menu);
  positionMenu(menu, x, y);
}

function positionMenu(menu, x, y) {
  const r = menu.getBoundingClientRect();
  menu.style.left = `${Math.min(x, window.innerWidth - r.width - 4)}px`;
  menu.style.top = `${Math.min(y, window.innerHeight - r.height - 4)}px`;
}

function buildMenu(entries) {
  const menu = h('div.menu');
  for (const e of entries) {
    if (!e) continue;
    if (e === '-') { menu.append(h('div.menu-sep')); continue; }
    if (e.head) { menu.append(h('div.menu-head', {}, e.head)); continue; }
    const it = h('div.menu-item', { class: e.disabled ? 'disabled' : null },
      h('span', { style: 'width: 12px' }, e.checked ? '✓' : ''), h('span.grow', {}, e.label), e.sub ? '▸' : (e.hint || ''));
    it.addEventListener('mouseenter', () => {
      if (menu.sub) { menu.sub.remove(); menu.sub = null; }
      if (!e.sub) return;
      const sub = buildMenu(e.sub);
      $('#menu-layer').append(sub);
      const r = it.getBoundingClientRect();
      positionMenu(sub, r.right - 2, r.top - 4);
      menu.sub = sub;
    });
    if (!e.sub) it.addEventListener('click', () => { closeMenu(); e.action(); });
    menu.append(it);
  }
  return menu;
}

function cellMenu(e, ctx, node) {
  const key = ctx.dashKey;
  const dash = getDash(key);
  const placement = `${key}/${node.id}`;
  const info = resolve(node.ref, ownerTaskOf(key));
  const common = [
    ctx.floating ? null : { label: 'Detach', action: () => detach(key, node.id) },
    ctx.floating ? null : { label: dev.maximised === placement ? 'Restore' : 'Maximise', hint: 'Ctrl+Shift+M', action: () => { setFocus(placement); toggleMaximise(); } },
    info.kind === 'item' && !ownerTaskOf(key) ? { label: 'Open its task', action: () => openPreview(info.task.id) } : null,
  ];
  if (!isUnlocked(key)) {
    showMenu(e.clientX, e.clientY, [{ head: 'The layout is locked' }, { label: 'Turn on editing', action: () => toggleLock(key) }, '-', ...common]);
    return;
  }
  showMenu(e.clientX, e.clientY, [
    { label: 'Split right', action: () => edit(key, (d) => splitCell(d, node.id, 'right', emptyCell())) },
    { label: 'Split down', action: () => edit(key, (d) => splitCell(d, node.id, 'bottom', emptyCell())) },
    { label: 'Wrap in tabs', action: () => edit(key, (d) => wrapInTabs(d, key, node.id)) },
    { label: 'Add tab', action: () => edit(key, (d) => addAsTab(d, key, node.id, emptyCell())) },
    '-',
    { label: ownerTaskOf(key) ? 'Change item…' : 'Change task or item…', action: () => changeRef(ctx, node) },
    { label: 'Clear cell', disabled: !node.ref, action: () => edit(key, () => { node.ref = null; }) },
    { label: 'Remove cell', disabled: dash.layout === node, action: () => edit(key, (d) => removeNode(d, node.id)) },
    '-',
    ...common,
  ]);
}

// Picks a new reference for a cell: an item of the task, or in a root dashboard any task or item.
function changeRef(ctx, node) {
  const owner = ownerTaskOf(ctx.dashKey);
  const set = (ref) => edit(ctx.dashKey, () => { node.ref = ref; });
  const entries = owner
    ? getTask(owner).items.map((i) => ({ kind: TYPES[i.type].name, label: i.name, action: () => set({ item: i.id }) }))
    : data.tasks.flatMap((t) => [
      { kind: 'Task', label: t.title, sub: whereOf(t), action: () => set({ task: t.id, dashboard: t.defaultDashboardId }) },
      ...t.items.map((i) => ({ kind: 'Item', label: i.name, sub: `${getProject(t.projectId).name} › ${t.title}`, action: () => set({ item: i.id, task: t.id }) })),
    ]);
  openPicker({ placeholder: owner ? 'Choose an item of this task' : 'Choose a task or an item', entries });
}

function tabMenu(e, dashKey, node, i) {
  if (!isUnlocked(dashKey)) {
    showMenu(e.clientX, e.clientY, [{ head: 'The layout is locked' }, { label: 'Turn on editing', action: () => toggleLock(dashKey) }]);
    return;
  }
  const move = (delta) => edit(dashKey, () => {
    const [tab] = node.tabs.splice(i, 1);
    node.tabs.splice(i + delta, 0, tab);
    node.current = i + delta;
  });
  showMenu(e.clientX, e.clientY, [
    { label: 'Rename', action: () => { const n = prompt('Tab title', node.tabs[i].title); if (n) edit(dashKey, () => { node.tabs[i].title = n; }); } },
    { label: 'Close', action: () => edit(dashKey, (d) => removeNode(d, node.tabs[i].node.id)) },
    { label: 'Move left', disabled: i === 0, action: () => move(-1) },
    { label: 'Move right', disabled: i === node.tabs.length - 1, action: () => move(1) },
    '-',
    { label: 'Collapse when single', checked: node.collapseSingle, action: () => edit(dashKey, () => { node.collapseSingle = !node.collapseSingle; }) },
  ]);
}

function rootTabMenu(e, d) {
  showMenu(e.clientX, e.clientY, [
    { label: 'Rename', action: () => { const n = prompt('Dashboard name', d.name); if (n) { d.name = n; commit(); } } },
    { label: 'Close tab', action: () => closeRootTab(d.id) },
    { label: 'Delete dashboard', action: () => {
      if (!confirm(`Delete the root dashboard “${d.name}”?`)) return;
      data.rootDashboards = data.rootDashboards.filter((x) => x !== d);
      closeRootTab(d.id);
    } },
  ]);
}

const priorityMenu = (entity) => PRIORITIES.map((name, n) => ({
  label: name, checked: entity.priority === n, action: () => { entity.priority = n; commit(); },
}));

function groupMenu(e, g) {
  showMenu(e.clientX, e.clientY, [
    { label: 'New project', action: () => {
      const name = prompt('Project name');
      if (!name) return;
      const p = { id: uid('p'), groupId: g.id, name, priority: 2, order: data.projects.length };
      data.projects.push(p);
      createTask(p, 'Default');
      dev.expanded[g.id] = true;
      dev.expanded[p.id] = true;
      commit();
    } },
    { label: 'Rename', action: () => { const n = prompt('Group name', g.name); if (n) { g.name = n; commit(); } } },
    { label: 'Delete', action: () => {
      if (!confirm(`Delete the group “${g.name}” with all its projects and tasks?`)) return;
      const projectIds = data.projects.filter((p) => p.groupId === g.id).map((p) => p.id);
      data.tasks = data.tasks.filter((t) => !projectIds.includes(t.projectId));
      data.projects = data.projects.filter((p) => p.groupId !== g.id);
      data.groups = data.groups.filter((x) => x !== g);
      commit();
    } },
  ]);
}

function projectMenu(e, p) {
  showMenu(e.clientX, e.clientY, [
    { label: 'New task', action: () => {
      const title = prompt('Task title');
      if (!title) return;
      const t = createTask(p, title);
      dev.expanded[p.id] = true;
      openPreview(t.id);
    } },
    { label: 'Rename', action: () => { const n = prompt('Project name', p.name); if (n) { p.name = n; commit(); } } },
    { label: 'Set priority', sub: priorityMenu(p) },
    '-',
    { label: 'Delete', action: () => {
      if (!confirm(`Delete the project “${p.name}” with all its tasks?`)) return;
      data.tasks = data.tasks.filter((t) => t.projectId !== p.id);
      data.projects = data.projects.filter((x) => x !== p);
      commit();
    } },
  ]);
}

function createTask(p, title) {
  const d = { id: uid('d'), name: 'Main', layout: emptyCell() };
  const t = {
    id: uid('t'), projectId: p.id, title, isDefault: title === 'Default', state: 'active', priority: 2,
    order: data.tasks.filter((x) => x.projectId === p.id).length, items: [], dashboards: [d], defaultDashboardId: d.id,
  };
  data.tasks.push(t);
  return t;
}

function addToDashboard(ref, label) {
  openPicker({
    placeholder: 'Add to which root dashboard?',
    entries: [
      ...data.rootDashboards.map((d) => ({ kind: 'Dashboard', label: d.name, action: () => startPlacing(ref, d.id, label) })),
      { kind: 'New', label: 'New root dashboard', action: () => { newRootDashboard(label, emptyCell(ref)); commit(); } },
    ],
  });
}

function taskMenu(e, t) {
  const def = t.isDefault;
  showMenu(e.clientX, e.clientY, [
    { label: 'Open', action: () => openPreview(t.id) },
    { label: 'Open in new root dashboard', action: () => { newRootDashboard(t.title, emptyCell({ task: t.id, dashboard: t.defaultDashboardId })); commit(); } },
    { label: 'Add to dashboard…', action: () => addToDashboard({ task: t.id, dashboard: t.defaultDashboardId }, t.title) },
    '-',
    { label: 'New item', sub: Object.entries(TYPES).map(([id, type]) => ({ label: type.name, action: () => {
      const name = prompt('Item name', type.name);
      if (!name) return;
      t.items.push({ id: uid('i'), type: id, name, config: id === 'web.page' ? { url: 'https://example.com' } : {} });
      dev.expanded[t.id] = true;
      commit();
    } })) },
    { label: 'Rename', disabled: def, action: () => { const n = prompt('Task title', t.title); if (n) { t.title = n; commit(); } } },
    { label: 'Set priority', disabled: def, sub: def ? null : priorityMenu(t) },
    { label: 'Set state', disabled: def, sub: def ? null : STATES.map((s) => ({ label: s, checked: t.state === s, action: () => { t.state = s; commit(); } })) },
    '-',
    { label: 'Delete', disabled: def, action: () => {
      if (!confirm(`Delete the task “${t.title}”? Cells showing it will show “Removed”.`)) return;
      data.tasks = data.tasks.filter((x) => x !== t);
      if (dev.preview && dev.preview.taskId === t.id) closePreview();
      commit();
    } },
  ]);
}

function itemMenu(e, t, item) {
  showMenu(e.clientX, e.clientY, [
    { label: 'Open its task', action: () => openPreview(t.id) },
    { label: 'Add to dashboard…', action: () => addToDashboard({ item: item.id, task: t.id }, item.name) },
    '-',
    { label: 'Rename', action: () => { const n = prompt('Item name', item.name); if (n) { item.name = n; commit(); } } },
    { label: 'Delete', action: () => {
      if (!confirm(`Delete the item “${item.name}”? Cells showing it will show “Removed”.`)) return;
      t.items = t.items.filter((x) => x !== item);
      commit();
    } },
  ]);
}

// ---------------------------------------------------------------- modals

function closeModal() {
  $('#modal-layer').replaceChildren();
}

const modalOpen = () => $('#modal-layer').children.length > 0;

// Fuzzy subsequence match. Returns a score, or -1 when `q` does not match.
function fuzzy(q, text) {
  if (!q) return 0;
  const t = text.toLowerCase();
  let score = 0;
  let pos = 0;
  let prev = -2;
  for (const ch of q.toLowerCase()) {
    if (ch === ' ') continue;
    const i = t.indexOf(ch, pos);
    if (i < 0) return -1;
    score += i === prev + 1 ? 3 : 1;
    if (i === 0 || /[\s›·\-_/]/.test(t[i - 1])) score += 2;
    prev = i;
    pos = i + 1;
  }
  return score - t.length * 0.01;
}

function openPicker({ placeholder, entries }) {
  closeMenu();
  let shown = [];
  let sel = 0;
  const input = h('input.search', { placeholder });
  const results = h('div.results');
  const pick = (entry) => { closeModal(); entry.action(); };
  const draw = () => {
    results.replaceChildren(...shown.map((en, i) => h('div.result', {
      class: i === sel ? 'sel' : null,
      onmousemove: () => { if (sel !== i) { sel = i; draw(); } },
      onclick: () => pick(en),
    }, h('span.kind', {}, en.kind || ''), h('span', {}, en.label), en.sub ? h('span.sub', {}, en.sub) : null)));
    const cur = results.children[sel];
    if (cur) cur.scrollIntoView({ block: 'nearest' });
  };
  const filter = () => {
    const q = input.value.trim();
    shown = entries.map((en, i) => ({ en, i, s: fuzzy(q, `${en.label} ${en.sub || ''} ${en.kind || ''}`) }))
      .filter((x) => x.s >= 0)
      .sort((a, b) => b.s - a.s || a.i - b.i)
      .slice(0, 60)
      .map((x) => x.en);
    sel = 0;
    draw();
  };
  input.addEventListener('input', filter);
  input.addEventListener('keydown', (e) => {
    if (e.key === 'ArrowDown') { sel = Math.min(sel + 1, shown.length - 1); draw(); e.preventDefault(); }
    else if (e.key === 'ArrowUp') { sel = Math.max(sel - 1, 0); draw(); e.preventDefault(); }
    else if (e.key === 'Enter') { if (shown[sel]) pick(shown[sel]); e.preventDefault(); }
    else if (e.key === 'Escape') { closeModal(); e.preventDefault(); }
    e.stopPropagation();
  });
  $('#modal-layer').replaceChildren(h('div.modal-back', { onmousedown: (e) => { if (e.target === e.currentTarget) closeModal(); } },
    h('div.modal', {}, input, results)));
  filter();
  input.focus();
}

function revealInSidebar(ids, flashId) {
  dev.sidebarCollapsed = false;
  ids.forEach((id) => { dev.expanded[id] = true; });
  commit();
  const row = document.querySelector(`.sidebar [data-id="${CSS.escape(flashId)}"]`);
  if (row) {
    row.scrollIntoView({ block: 'nearest' });
    row.classList.add('selected');
    setTimeout(() => row.classList.remove('selected'), 1200);
  }
}

function openSwitcher() {
  const entries = [];
  for (const g of groupsSorted()) {
    entries.push({ kind: 'Group', label: g.name, action: () => revealInSidebar([g.id], g.id) });
  }
  for (const p of data.projects) {
    entries.push({ kind: 'Project', label: p.name, sub: getGroup(p.groupId).name, action: () => revealInSidebar([p.groupId, p.id], p.id) });
  }
  for (const t of data.tasks) {
    entries.push({ kind: 'Task', label: t.title, sub: whereOf(t), action: () => openPreview(t.id) });
  }
  for (const t of data.tasks) {
    for (const i of t.items) {
      entries.push({ kind: 'Item', label: i.name, sub: `${getProject(t.projectId).name} › ${t.title}`, action: () => {
        openPreview(t.id);
        const el = document.querySelector(`.main .content .cell[data-item="${CSS.escape(i.id)}"]`);
        if (el) setFocus(el.dataset.placement);
        else toast(`“${i.name}” is not in the task's current dashboard`);
      } });
    }
  }
  for (const d of data.rootDashboards) {
    entries.push({ kind: 'Dashboard', label: d.name, sub: 'root dashboard', action: () => activateRoot(d.id) });
  }
  for (const t of data.tasks) {
    for (const d of t.dashboards) {
      entries.push({ kind: 'Dashboard', label: d.name, sub: `task: ${t.title}`, action: () => openPreview(t.id, d.id) });
    }
  }
  openPicker({ placeholder: 'Go to a group, project, task, item or dashboard…', entries });
}

const SHORTCUTS = [
  ['Ctrl+K or Alt+K', 'Quick switcher'],
  ['Ctrl+Alt+Arrow', 'Move focus to the cell in that direction'],
  ['Ctrl+Tab, Ctrl+Shift+Tab, or Alt+] and Alt+[', 'Next or previous tab in the focused tab group'],
  ['Ctrl+PgDown, Ctrl+PgUp, or Alt+PgDown and Alt+PgUp', 'Next or previous root dashboard tab'],
  ['Ctrl+Shift+M or Alt+M', 'Maximise the focused cell, or restore it'],
  ['Ctrl+W or Alt+W', 'Close the preview tab or the focused root tab'],
  ['Esc', 'Close menus, cancel placing, restore a maximised cell'],
  ['? or F1', 'This help'],
];

function openHelp() {
  closeMenu();
  $('#modal-layer').replaceChildren(h('div.modal-back', { onmousedown: (e) => { if (e.target === e.currentTarget) closeModal(); } },
    h('div.modal', {}, h('div.help', {},
      h('h4', {}, 'Keyboard'),
      h('p.muted', {}, 'Browsers keep some Ctrl shortcuts for themselves, so the prototype also accepts the Alt variants. The desktop app will use the Ctrl ones.'),
      h('table', {}, SHORTCUTS.map(([k, v]) => h('tr', {}, h('td', {}, h('kbd', {}, k)), h('td', {}, v)))),
      h('h4.mt', {}, 'Mouse'),
      h('ul', {},
        h('li', {}, 'Click a task to preview it. Double-click, the pin button, or turning on editing keeps it as a tab.'),
        h('li', {}, 'Drag a task or item from the sidebar onto a dashboard. Edges split, the centre adds a tab.'),
        h('li', {}, 'With editing on, drag cells by their header, and right-click cells and tabs for more.'),
        h('li', {}, 'Drag splitters to resize, even when the layout is locked.'),
        h('li', {}, 'Drag tasks and projects in the sidebar to reorder them. Dropping next to another level changes the priority.'))))));
}

// ---------------------------------------------------------------- drag and drop onto layouts

// Converts what is being dragged into a cell reference for the target dashboard.
// Returns undefined when the dashboard cannot take it.
function refForTarget(targetKey) {
  const owner = ownerTaskOf(targetKey);
  let taskId;
  let itemId;
  let dashboard;
  if (drag.kind === 'task') {
    taskId = drag.taskId;
    dashboard = getTask(taskId).defaultDashboardId;
  } else if (drag.kind === 'item') {
    taskId = drag.taskId;
    itemId = drag.itemId;
  } else if (drag.kind === 'cell') {
    const info = resolve(drag.ref, ownerTaskOf(drag.dashKey));
    if (info.kind === 'item') { taskId = info.task.id; itemId = info.item.id; }
    else if (info.kind === 'task') { taskId = info.task.id; dashboard = drag.ref.dashboard; }
    else return null;
  } else {
    return undefined;
  }
  if (itemId) {
    if (owner && owner !== taskId) return undefined;
    return owner ? { item: itemId } : { item: itemId, task: taskId };
  }
  return owner ? undefined : { task: taskId, dashboard };
}

function dropTargetAt(e) {
  const el = e.target instanceof Element ? e.target : null;
  if (!el || el.closest('.sidebar') || el.closest('#floating')) return null;
  const bar = el.closest('.tabbar');
  if (bar) {
    const ref = refForTarget(bar.dataset.dash);
    if (ref === undefined) return null;
    const tab = el.closest('.tab');
    let index;
    let rect;
    if (tab) {
      const r = tab.getBoundingClientRect();
      const after = e.clientX > r.left + r.width / 2;
      index = Number(tab.dataset.index) + (after ? 1 : 0);
      rect = { left: (after ? r.right : r.left) - 2, top: r.top, width: 4, height: r.height };
    } else {
      const fill = bar.querySelector('.fill').getBoundingClientRect();
      index = bar.querySelectorAll('.tab').length;
      rect = { left: fill.left, top: fill.top, width: 4, height: fill.height };
    }
    return { kind: 'tab', dashKey: bar.dataset.dash, tabsId: bar.dataset.node, index, ref, rect };
  }
  let cell = el.closest('.cell[data-dash]');
  while (cell) {
    const self = drag.kind === 'cell' && cell.dataset.dash === drag.dashKey && cell.dataset.node === drag.nodeId;
    const ref = self ? undefined : refForTarget(cell.dataset.dash);
    if (ref !== undefined) {
      const r = cell.getBoundingClientRect();
      const x = (e.clientX - r.left) / r.width;
      const y = (e.clientY - r.top) / r.height;
      const dist = { left: x, right: 1 - x, top: y, bottom: 1 - y };
      let zone = 'centre';
      let min = 0.25;
      for (const k of Object.keys(dist)) if (dist[k] < min) { min = dist[k]; zone = k; }
      const rects = {
        left: { left: r.left, top: r.top, width: r.width / 2, height: r.height },
        right: { left: r.left + r.width / 2, top: r.top, width: r.width / 2, height: r.height },
        top: { left: r.left, top: r.top, width: r.width, height: r.height / 2 },
        bottom: { left: r.left, top: r.top + r.height / 2, width: r.width, height: r.height / 2 },
        centre: { left: r.left + r.width * 0.2, top: r.top + r.height * 0.2, width: r.width * 0.6, height: r.height * 0.6 },
      };
      return { kind: 'cell', dashKey: cell.dataset.dash, nodeId: cell.dataset.node, zone, ref, rect: rects[zone] };
    }
    cell = cell.parentElement && cell.parentElement.closest('.cell[data-dash]');
  }
  return null;
}

function showHint(r) {
  const el = $('#drop-hint');
  Object.assign(el.style, { display: 'block', left: `${r.left}px`, top: `${r.top}px`, width: `${r.width}px`, height: `${r.height}px` });
}

function hideHint() {
  $('#drop-hint').style.display = 'none';
}

function applyDrop(t) {
  const dash = getDash(t.dashKey);
  const keys = [t.dashKey];
  if (drag.kind === 'cell') {
    const src = getDash(drag.dashKey);
    const f = src && findNode(src.layout, drag.nodeId);
    if (f) {
      if (t.kind === 'tab' && f.parent && f.parent.id === t.tabsId && f.index < t.index) t.index -= 1;
      removeNode(src, drag.nodeId);
      if (!keys.includes(drag.dashKey)) keys.push(drag.dashKey);
    }
  }
  const newNode = emptyCell(t.ref);
  if (t.kind === 'tab') {
    insertTab(dash, t.dashKey, t.tabsId, t.index, newNode);
  } else {
    const f = findNode(dash.layout, t.nodeId);
    if (!f) return;
    if (t.zone === 'centre' && f.node.type === 'cell' && f.node.ref === null) f.node.ref = t.ref;
    else if (t.zone === 'centre') addAsTab(dash, t.dashKey, t.nodeId, newNode);
    else splitCell(dash, t.nodeId, t.zone, newNode);
  }
  afterLayoutEdit(keys);
}

document.addEventListener('dragover', (e) => {
  if (!drag) return;
  const t = dropTargetAt(e);
  dropTarget = t;
  if (!t) { hideHint(); return; }
  e.preventDefault();
  e.dataTransfer.dropEffect = drag.kind === 'cell' ? 'move' : 'copy';
  showHint(t.rect);
});

document.addEventListener('drop', (e) => {
  hideHint();
  if (!drag || !dropTarget) return;
  e.preventDefault();
  const t = dropTarget;
  applyDrop(t);
  drag = null;
  dropTarget = null;
});

document.addEventListener('dragend', () => {
  drag = null;
  dropTarget = null;
  hideHint();
  document.querySelectorAll('.drop-before, .drop-after').forEach((el) => el.classList.remove('drop-before', 'drop-after'));
});

// ---------------------------------------------------------------- keyboard and global events

document.addEventListener('keydown', (e) => {
  if (modalOpen()) {
    if (e.key === 'Escape') closeModal();
    return;
  }
  const ctrl = e.ctrlKey || e.metaKey;
  const typing = e.target instanceof Element && e.target.matches('input, textarea, select');
  const key = e.key.toLowerCase();
  const handled = () => { e.preventDefault(); e.stopPropagation(); };

  if (e.key === 'Escape') {
    closeMenu();
    if (placing) { placing = null; render(); }
    if (dev.maximised) { dev.maximised = null; commit(); }
    return;
  }
  if ((ctrl || e.altKey) && !e.shiftKey && key === 'k') { handled(); openSwitcher(); return; }
  if (ctrl && e.shiftKey && key === 'n') { handled(); toast('Quick add is not part of the prototype'); return; }
  if (ctrl && e.altKey && e.key.startsWith('Arrow')) { handled(); moveFocus(e.key.slice(5).toLowerCase()); return; }
  if ((ctrl && e.key === 'Tab') || (e.altKey && (e.key === ']' || e.key === '['))) {
    handled();
    cycleInnerTab(e.key === '[' || e.shiftKey ? -1 : 1);
    return;
  }
  if ((ctrl || e.altKey) && (e.key === 'PageDown' || e.key === 'PageUp')) { handled(); cycleRootTab(e.key === 'PageDown' ? 1 : -1); return; }
  if ((ctrl && e.shiftKey && key === 'm') || (e.altKey && !ctrl && key === 'm')) { handled(); toggleMaximise(); return; }
  if ((ctrl || e.altKey) && !e.shiftKey && key === 'w') { handled(); closeActiveTab(); return; }
  if (!typing && (e.key === '?' || e.key === 'F1')) { handled(); openHelp(); }
}, true);

document.addEventListener('mousedown', (e) => {
  if (!e.target.closest('.menu')) closeMenu();
});

window.addEventListener('resize', () => {
  if (dev.maximised) applyMaximised();
});

load();
render();
