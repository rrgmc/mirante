# 0041. Mirante draws its own tabs and tool buttons

- Status: Accepted
- Date: 2026-10-29

## Context

On macOS, Qt's platform style frames every tool button, even an auto-raise one outside a toolbar. It also fills the current tab with the accent colour and puts the tab's close button on the left. Mirante has many tool buttons and tab bars, so on a Mac it looked like a Windows app.

## Decision

An application style, `AppStyle`, wraps the platform style in a `QProxyStyle`. It draws auto-raise tool buttons outside toolbars, tabs and tab close buttons as quiet rounded fills, in the same way on every platform. Every other control keeps the platform look. Cell borders become lighter and rounder, and the warning bar loses its frame. The details are in [the user interface](../ui.md#look).

## Alternatives

- **Style sheets.** They would cover the same controls, but a style sheet on a tool button or tab bar replaces its platform drawing completely, it needs colours written out for light and dark themes, and it mixes badly with the style sheets the cells already use.
- **Native macOS controls in `src/platform/`.** Hosting `NSSegmentedControl` or `NSTabView` in Qt widgets is much more work, and Windows and Linux would still need their own fix.
- **Changing only macOS.** The flat look also suits Windows and Linux, and one look is less to test.

## Consequences

- Tabs and tool buttons look the same on every platform, which makes screenshots and documentation simpler.
- The look of these controls no longer follows platform updates; `AppStyle` must be updated by hand.
- A translucent sidebar and a unified title bar and toolbar on macOS remain possible later, in `src/platform/`.
