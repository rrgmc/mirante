# 0060. Jira Cloud or Data Center is a setting, guessed from the site

- Status: Accepted
- Date: 2026-10-29

## Context

Jira Cloud and Jira Data Center have different APIs and log in differently. See [integrations](../integrations.md#jira). Before [decision 0054](0054-module-settings-replace-connections.md) the connection kind, `jira.cloud` or `jira.dc`, told them apart. Decision 0054 removed connections and left the question to M5. Jira Cloud also needs the account's email address, because its API token is sent with Basic authentication as `email:token`.

## Decision

- **A Deployment setting.** Jira's General Configuration gains **Deployment**, a choice of "Automatic", "Jira Cloud" and "Jira Data Center", stored as `jira.general.deployment` with the values `auto`, `cloud` and `dc`. The default is `auto`.
- **Automatic** means Jira Cloud when the base URL's host ends in `.atlassian.net` or `.jira.com`, and Jira Data Center otherwise.
- **An Email setting.** The same screen gains **Email**, `jira.general.email`, a plain text field. It is used only for Jira Cloud. It is not a secret, so it is stored and synced with the other settings.
- **Missing email.** With Jira Cloud and no email, every request fails before it is sent, and the item shows "Set the email for Jira Cloud in the profile's Jira settings." with a button that opens the screen.
- **Paths.** The jira module builds its API paths from the deployment: `/rest/api/3` on Cloud, `/rest/api/2` on Data Center. The core does not know the difference.

## Alternatives

- **Always guess from the address.** No setting to explain. But Jira Cloud also runs on custom domains, and a Data Center site could be behind any host name; a wrong guess would have no way out.
- **Ask the server.** `GET /rest/api/2/serverInfo` returns `deploymentType`. It costs a request before the first real one, needs a token that works on both, and fails while the token is wrong, which is exactly when the user needs a clear message.
- **Two modules, `jiracloud` and `jiradc`.** Two sets of item types for one product, and existing items would need a new type.
- **The email as part of the token, typed as `email:token`.** One field fewer, but easy to get wrong, and the email would land in the keychain where it cannot be seen or checked.

## Consequences

- Most users never touch Deployment.
- A profile that talks to a Jira Cloud site on its own domain chooses "Jira Cloud" once.
- The email is visible to everyone who shares the workspace. It is not secret, but it is personal data in synced files.
