// Mock data for the M1 prototype. Shapes follow docs/data-model.md.
// Ids are short strings instead of UUIDs, to keep the data readable.
// `mock` on an item stands in for live data from the status cache.

(function () {
  const H = (sizes, ...children) => ({ type: 'split', dir: 'h', sizes, children });
  const V = (sizes, ...children) => ({ type: 'split', dir: 'v', sizes, children });
  const T = (...tabs) => ({
    type: 'tabs', current: 0, collapseSingle: false,
    tabs: tabs.map(([title, node]) => ({ title, node })),
  });
  const I = (item) => ({ type: 'cell', ref: { item } });
  const TK = (task, dashboard) => ({ type: 'cell', ref: { task, dashboard } });
  const RI = (task, item) => ({ type: 'cell', ref: { item, task } });

  const dash = (id, name, layout) => ({ id, name, layout });

  const task = (id, projectId, title, priority, state, order, items, dashboards) => ({
    id, projectId, title, isDefault: title === 'Default', state, priority, order,
    items, dashboards, defaultDashboardId: dashboards[0].id,
  });

  const groups = [
    { id: 'g-work', name: 'Work', order: 0 },
    { id: 'g-personal', name: 'Personal', order: 1 },
  ];

  const projects = [
    { id: 'p-backend', groupId: 'g-work', name: 'backend', priority: 1, order: 0, gitlabProject: 'acme/backend', jiraProjectKey: 'ACME' },
    { id: 'p-frontend', groupId: 'g-work', name: 'frontend', priority: 2, order: 0, gitlabProject: 'acme/frontend', jiraProjectKey: 'ACME' },
    { id: 'p-infra', groupId: 'g-work', name: 'infra', priority: 3, order: 0, gitlabProject: 'acme/infra', jiraProjectKey: 'OPS' },
    { id: 'p-rraidash', groupId: 'g-personal', name: 'rraidash', priority: 2, order: 0, gitlabProject: 'rr/rraidash' },
    { id: 'p-home', groupId: 'g-personal', name: 'home', priority: 3, order: 0 },
  ];

  const tasks = [
    // backend
    task('t-be-default', 'p-backend', 'Default', 2, 'active', 0, [
      { id: 'be-mrs', type: 'gitlab.mr.list', name: 'Review requested', config: { preset: 'Review requested from me' }, mock: { attention: ['Review requested on !233'] } },
      { id: 'be-sprint', type: 'jira.list', name: 'Current sprint', config: { preset: 'Current sprint of this project' } },
      { id: 'be-main-ci', type: 'gitlab.pipeline', name: 'main pipeline', config: { branch: 'main' }, mock: { pipeline: 'passed' } },
    ], [
      dash('d-be-default', 'Overview', H([1, 1], I('be-mrs'), V([1, 1], I('be-sprint'), I('be-main-ci')))),
    ]),
    task('t-login', 'p-backend', 'Fix login timeout', 0, 'active', 0, [
      { id: 'login-claude', type: 'claude.session.web', name: 'Claude: login fix', config: { url: 'https://claude.ai/code/session_01AbC' } },
      { id: 'login-jira', type: 'jira.issue.summary', name: 'ACME-412', config: { key: 'ACME-412' }, mock: { summary: 'Login times out after 30 s behind the proxy', status: 'In Progress' } },
      { id: 'login-jira-page', type: 'jira.issue.page', name: 'ACME-412 (page)', config: { url: 'https://acme.atlassian.net/browse/ACME-412' } },
      { id: 'login-mr', type: 'gitlab.mr.summary', name: '!231 Fix session refresh', config: { project: 'acme/backend', iid: 231 }, mock: { pipeline: 'failed', attention: ['Pipeline failed'] } },
      { id: 'login-mr-page', type: 'gitlab.mr.page', name: '!231 (page)', config: { url: 'https://gitlab.com/acme/backend/-/merge_requests/231' } },
      { id: 'login-ci', type: 'gitlab.pipeline', name: 'Pipeline: fix/login-timeout', config: { branch: 'fix/login-timeout' }, mock: { pipeline: 'failed', attention: ['Pipeline failed'] } },
      { id: 'login-folder', type: 'folder', name: 'backend folder', config: { folder: 'backend' } },
      { id: 'login-notes', type: 'notes', name: 'Notes', config: { text: '- Proxy drops idle connections after 30 s\n- Try keep-alive on the token refresh call' } },
    ], [
      dash('d-login-work', 'Work', H([2, 1],
        I('login-claude'),
        T(['Issue', V([1, 1], I('login-jira'), I('login-mr'))], ['Pipeline', I('login-ci')], ['Notes', I('login-notes')]))),
      dash('d-login-review', 'Review', H([1, 1], I('login-mr-page'), V([2, 1], I('login-mr'), I('login-folder')))),
    ]),
    task('t-ratelimit', 'p-backend', 'Add rate limiting', 1, 'active', 0, [
      { id: 'rl-jira', type: 'jira.issue.summary', name: 'ACME-398', config: { key: 'ACME-398' }, mock: { summary: 'Rate limit the public API per token', status: 'In Review', attention: ['Status changed to In Review'] } },
      { id: 'rl-mr', type: 'gitlab.mr.summary', name: '!228 Token bucket limiter', config: { project: 'acme/backend', iid: 228 }, mock: { pipeline: 'passed', attention: ['Approved'] } },
      { id: 'rl-claude', type: 'claude.session.local', name: 'Claude (local)', config: { sessionId: '8f2c1e', device: 'desk-pc' } },
    ], [
      dash('d-rl', 'Main', H([1, 1], I('rl-claude'), V([1, 1], I('rl-jira'), I('rl-mr')))),
    ]),
    task('t-flaky', 'p-backend', 'Investigate flaky test', 3, 'active', 0, [
      { id: 'flaky-ci', type: 'gitlab.pipeline', name: 'Pipeline: main', config: { branch: 'main' }, mock: { pipeline: 'passed' } },
      { id: 'flaky-notes', type: 'notes', name: 'Notes', config: { text: 'TestSessionExpiry fails about 1 in 20 runs.' } },
    ], [
      dash('d-flaky', 'Main', T(['Pipeline', I('flaky-ci')], ['Notes', I('flaky-notes')])),
    ]),
    task('t-go', 'p-backend', 'Upgrade Go to 1.23', 2, 'done', 1, [
      { id: 'go-mr', type: 'gitlab.mr.summary', name: '!219 Go 1.23', config: { project: 'acme/backend', iid: 219 }, mock: { pipeline: 'passed', state: 'merged' } },
    ], [
      dash('d-go', 'Main', I('go-mr')),
    ]),

    // frontend
    task('t-fe-default', 'p-frontend', 'Default', 2, 'active', 0, [
      { id: 'fe-mrs', type: 'gitlab.mr.list', name: 'Assigned to me', config: { preset: 'Assigned to me' } },
      { id: 'fe-issues', type: 'gitlab.issue.list', name: 'Open issues', config: { preset: 'All open' } },
    ], [
      dash('d-fe-default', 'Overview', V([1, 1], I('fe-mrs'), I('fe-issues'))),
    ]),
    task('t-dark', 'p-frontend', 'Dark mode toggle', 2, 'active', 0, [
      { id: 'dark-issue', type: 'gitlab.issue.summary', name: '#87 Dark mode', config: { iid: 87 } },
      { id: 'dark-issue-page', type: 'gitlab.issue.page', name: '#87 (page)', config: { url: 'https://gitlab.com/acme/frontend/-/issues/87' } },
      { id: 'dark-mr', type: 'gitlab.mr.summary', name: '!112 Theme switch', config: { project: 'acme/frontend', iid: 112 }, mock: { pipeline: 'running', attention: ['Review requested from you'] } },
      { id: 'dark-figma', type: 'web.page', name: 'Figma: themes', config: { url: 'https://figma.com/file/acme-themes' } },
    ], [
      dash('d-dark', 'Grid', V([1, 1], H([1, 1], I('dark-issue'), I('dark-mr')), H([1, 1], I('dark-figma'), I('dark-issue-page')))),
    ]),
    task('t-a11y', 'p-frontend', 'Accessibility audit', 2, 'active', 1, [
      { id: 'a11y-page', type: 'web.page', name: 'WCAG checklist', config: { url: 'https://www.w3.org/WAI/WCAG22/quickref/' } },
      { id: 'a11y-notes', type: 'notes', name: 'Findings', config: { text: '' } },
    ], [
      dash('d-a11y', 'Main', H([3, 2], I('a11y-page'), I('a11y-notes'))),
    ]),

    // infra
    task('t-infra-default', 'p-infra', 'Default', 2, 'active', 0, [
      { id: 'infra-ci', type: 'gitlab.pipeline', name: 'Deploy pipeline', config: { branch: 'main' }, mock: { pipeline: 'passed' } },
      { id: 'infra-overview', type: 'dash.panel', name: 'Work overview', config: { panel: 'Group overview', groupId: 'g-work' } },
    ], [
      dash('d-infra-default', 'Overview', H([1, 1], I('infra-ci'), I('infra-overview'))),
    ]),
    task('t-certs', 'p-infra', 'Rotate TLS certificates', 1, 'active', 0, [
      { id: 'certs-jira', type: 'jira.issue.summary', name: 'OPS-77', config: { key: 'OPS-77' }, mock: { summary: 'Certificates expire on 15 November', status: 'To Do' } },
      { id: 'certs-folder', type: 'folder', name: 'infra folder', config: { folder: 'infra' } },
    ], [
      dash('d-certs', 'Main', H([1, 1], I('certs-jira'), I('certs-folder'))),
    ]),
    task('t-tf', 'p-infra', 'Old Terraform cleanup', 2, 'archived', 0, [], [
      dash('d-tf', 'Main', { type: 'cell', ref: null }),
    ]),

    // rraidash
    task('t-rr-default', 'p-rraidash', 'Default', 2, 'active', 0, [
      { id: 'rr-folder', type: 'folder', name: 'rraidash folder', config: { folder: 'rraidash' } },
      { id: 'rr-notes', type: 'notes', name: 'Ideas', config: { text: '- Calendar item type?\n- Pomodoro timer in the tray' } },
    ], [
      dash('d-rr-default', 'Overview', H([1, 1], I('rr-folder'), I('rr-notes'))),
    ]),
    task('t-m1', 'p-rraidash', 'M1 HTML prototype', 0, 'active', 0, [
      { id: 'm1-claude', type: 'claude.session.local', name: 'Claude (local)', config: { sessionId: 'a41d09', device: 'desk-pc' } },
      { id: 'm1-docs', type: 'web.page', name: 'ui.md', config: { url: 'https://gitlab.com/rr/rraidash/-/blob/master/docs/ui.md' } },
      { id: 'm1-notes', type: 'notes', name: 'Notes', config: { text: '' } },
    ], [
      dash('d-m1', 'Main', H([1, 1], I('m1-claude'), T(['Docs', I('m1-docs')], ['Notes', I('m1-notes')]))),
    ]),
    task('t-m2', 'p-rraidash', 'M2 Qt spike', 2, 'active', 1, [
      { id: 'm2-notes', type: 'notes', name: 'Notes', config: { text: '' } },
    ], [
      dash('d-m2', 'Main', I('m2-notes')),
    ]),

    // home
    task('t-home-default', 'p-home', 'Default', 2, 'active', 0, [
      { id: 'home-notes', type: 'notes', name: 'Notes', config: { text: '' } },
    ], [
      dash('d-home-default', 'Overview', I('home-notes')),
    ]),
    task('t-tax', 'p-home', 'Tax documents', 1, 'active', 0, [
      { id: 'tax-page', type: 'web.page', name: 'Tax portal', config: { url: 'https://tax.example.gov' } },
      { id: 'tax-notes', type: 'notes', name: 'Checklist', config: { text: '- [ ] Bank statements\n- [ ] Receipts' } },
    ], [
      dash('d-tax', 'Main', H([2, 1], I('tax-page'), I('tax-notes'))),
    ]),
    task('t-garden', 'p-home', 'Garden plan', 3, 'active', 1, [
      { id: 'garden-notes', type: 'notes', name: 'Plan', config: { text: '' } },
    ], [
      dash('d-garden', 'Main', I('garden-notes')),
    ]),
  ];

  const rootDashboards = [
    { id: 'r-morning', name: 'Morning', order: 0, layout: H([3, 2],
      TK('t-login', 'd-login-work'),
      T(['Reviews', RI('t-be-default', 'be-mrs')], ['Sprint', RI('t-be-default', 'be-sprint')], ['Overview', RI('t-infra-default', 'infra-overview')])) },
    { id: 'r-release', name: 'Release', order: 1, layout: V([2, 1],
      H([1, 1], TK('t-ratelimit', 'd-rl'), TK('t-certs', 'd-certs')),
      RI('t-be-default', 'be-main-ci')) },
  ];

  window.MOCK = { groups, projects, tasks, rootDashboards };
})();
