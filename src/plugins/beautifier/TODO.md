# GTK 4 port

A lot of this plugin needs to be refactored given how much has changed
in Builder since it's inception.

 * It should probably use a buffer addin to provide the beautify actions.
 * It should probably use a dyanmic GMenu attached to the sourceview context menu, attached from an IdeEditorPageAddin.
 * It should add an IdeShortcutProvider to wire up dynamic shortcuts.
 * It should use IdeWorkspaceAddin instead of IdeEditorAddin to track pages.
 * It probably needs a preferences addin to setup beautifiers.
