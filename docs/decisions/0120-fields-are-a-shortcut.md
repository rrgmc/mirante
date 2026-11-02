# 0120. Declared fields are a shortcut, and every configuration screen can be custom

- Status: Accepted
- Date: 2026-11-02

## Context

A module's configuration screens are built from `Field` declarations, with a fixed set of types. The user's experience with systems like this is that they become hostile when fields are the only option, because an author who needs something else is stuck.

`Field` does two jobs. It describes the data, for `validate()`, `defaultConfig()` and finding secrets, and it describes the screen, through `FieldForm`. Only the second job limits the interface. Today, item configuration and module settings screens accept a custom editor, but project settings and device settings do not. `FieldForm` is in `src/ui`, so a module cannot use it inside its own editor.

## Decision

- **Every screen a module can add also accepts a fully custom one.** Project settings and device settings gain custom editors, as item configuration and settings screens have.
- **`FieldForm` and its editors move into the view kit,** so a custom editor can combine automatic sections with its own widgets.
- **A single field can have a custom widget,** and the rest of the form stays automatic.
- **`Field` stays as the description of the data, and grows little:** help text, groups and possibly a condition for showing a field. Anything more complex uses a custom editor.
- **Custom editors can do what `FieldForm` can,** including secrets, validation messages, folder and image pickers, and knowing the profile being edited. `FieldForm` uses only the public interface.
- **The core keeps the dialog's frame,** such as its buttons and the project dialog's group box.
- **This is separate work,** done before the module interface reaches 1.0 and not part of the sample of [decision 0116](0116-custom-builds-in-separate-repositories.md).

## Alternatives

- **Only a richer `Field`.** It keeps windows consistent, but every new need becomes a request to the core, and `Field` slowly becomes a language for describing screens.
- **Only custom editors, without `Field`.** This is the most flexible, but every module would write its own validation, defaults and secret handling.
- **HTML settings pages.** They would not match the native windows.
- **Qt Designer forms bound by widget name,** as KDE does with `KConfigDialogManager`. This is not rejected. It can be added later as a helper, with no change to the interface.

## Consequences

- A module author is never blocked by the set of field types.
- `FieldForm` becomes part of the public interface, so its shape should settle before 1.0.
- Windows can look less uniform when modules use custom editors. The view kit makes the consistent choice the easy one.
