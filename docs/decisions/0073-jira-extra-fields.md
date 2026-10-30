# 0073. Jira's extra fields are named in the profile's settings

- Status: Accepted
- Date: 2026-10-30

## Context

The user's Jira site has important fields, such as "Team" and "Fix version". Their ids differ per site, and a custom field's value can be text, an object or a list.

## Decision

- The Jira General Configuration gains **Extra fields**, a text list of field names, one per line.
- A summary finds each by name, ignoring case, in the `names` that its issue request already asks for, and shows it in Details. A name the site does not have is left out quietly.
- A value is read generically: text and numbers as they are; an object by its `name`, `value` or `displayName`; a list joined with commas.
- Lists also show them, as extra columns that hide first in a narrow cell.

## Alternatives

- **A field id per field.** Exact, but the user would have to look the ids up.
- **Per item.** More flexible, but every item would repeat the same names.

## Consequences

- Two fields with the same name on a site show the first one found.
- A list needs the ids of those fields, so it asks for `names` on its search as well.
