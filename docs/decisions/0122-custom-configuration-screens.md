# 0122. How a module gives a custom configuration screen

- Status: Accepted
- Date: 2026-11-02

## Context

[Decision 0120](0120-fields-are-a-shortcut.md) says every screen a module can add also accepts a custom one, and that custom editors can do what `FieldForm` can. This decision records how. Decision [0121](0121-modules-are-trusted-code.md) removes raw token access from the interface, so a custom editor's access to tokens needed its own answer.

## Decision

- **One kind of editor.** All custom screens are an `ItemEditor`. `SettingsScreen::createEditor()` takes an `EditorContext`, as `ItemType::createEditor()` does. Project and device settings take a factory, with `ModuleRegistrar::setProjectEditor()` and `setDeviceEditor()`.
- **`Field` still describes the data.** A custom editor replaces the form only. A custom settings screen lists its tokens as `Secret` fields, and the core uses that list to load their state and to delete them with their profile. It ignores a token for any other key. A custom device editor declares its keys, because `ItemContext::deviceSetting()` answers only declared keys.
- **An editor can write a token but never read it.** `ItemEditor` has `secrets()`, `removedSecrets()` and `setSecretState()`. The core works through these for any editor.
- **Tokens stay on settings screens.** A token's key holds a profile, and a project or a device has no profile of its own. The core ignores `secrets()` from a project or device editor.
- **An editor knows its profile.** `EditorContext::profileId` gives it at the start, and `ItemEditor::setProfile()` reports a change, such as another profile in the item dialog or another group in the project dialog. The core's own forms are built again instead, as before.
- **The settings window saves on `ItemEditor::changed()`,** so a device editor must emit it.
- **One field can have its own editor,** with `FieldForm::setFieldEditor()` and a `FieldEditor`. A type returns a `FieldForm` from `createEditor()` and sets the field. `Field` holds no widget factory, because it is a description of data. A `Secret` field cannot be replaced this way, because a `FieldEditor` holds a value and a token must not become one.
- **`Field` gains `help` and `group`,** and no condition for showing a field. No screen needs a condition yet.
- **Device settings take more types.** Every type except `Secret`, `Folder` and `Image`, which need a profile or a project. This replaces the list in [decision 0079](0079-module-device-settings.md). `DeviceSettings` already stores any JSON value.

## Alternatives

- **A widget factory in `Field`.** The automatic form would pick it up with no `createEditor()`, but every copy of a field would carry UI code, against decision 0120.
- **Tokens in project or device editors.** They would need new key formats and deletion rules. Project tokens also would not sync, so they would be missing on other devices.
- **A list of secret keys on `ItemEditor`.** The core needs the keys when a profile is deleted, when no editor exists. `SettingsScreen::fields()` already answers this.

## Consequences

- A forgotten `Secret` field means a token is not stored, with a warning in the log, rather than a token left behind in the keychain.
- No first-party module uses these abilities yet, so only tests exercise them until one does.
