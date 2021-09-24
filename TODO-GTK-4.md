# GTK 4 Notes

This document is meant to help keep track of things that we will need to come back to as part of the GTK 4 port.

## IdeTree

 * DnD drop targets need work, as they aren't being handled yet. This will be easier with plugins to test it.
 * The cell renderer uses cairo drawing. Instead we should use icons for the Git states so we get GDK texture caching.

## IdeRunManager

 * We don't have a short manager for GTK 4 yet. We will want an accessory object for IdeApplication or similar. Alternatively, the workspace should monitor the IdeRunManager for changes and add/remove shortcuts.

## IdeSurface

 * We should rename this to IdeWorkspaceContent as it fits better with the `content` metaphor AdwApplicationWindow.
 * Alternatively, we can probably just drop IdeSurface altogether. Instead, project properties could either be shown in a page or with a new dialog like we do for preferences (or a workspace window so it gets access to IdeContext).


