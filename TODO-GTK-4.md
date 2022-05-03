# GTK 4 Notes

This document is meant to help keep track of things that we will need to come back to as part of the GTK 4 port.

## IdeTree

 * DnD drop targets need work, as they aren't being handled yet. This will be easier with plugins to test it.
 * The cell renderer uses cairo drawing. Instead we should use icons for the Git states so we get GDK texture caching.

## Shortcuts

We still need a Shortcuts manager and a way to define shortcuts, especially by users.
My plan so far is to have an IdeShortcut which is has a few things in it:

 * "action" or "command" property of what to activate
 * "params" which is the GVariant parameters
 * "when" which is a simple set of rules we can apply if we can activate it
   such as `inWorkspace(primary)` or `inPage(editor)` or `hasProject()`.
 * A "phase" that allows changing bubble/capture semantics
 * A list model to filter all of these out based on workspace status/focus/etc
 * A custom GtkShortcutController to apply these rules from the model

## Editor Search

This still hasn't been brought over because it needs a lot of changes for
GTK 4. However, most of that was already done on GNOME Text Editor. We also
probably want to change the layout based on updated designs from Allan.

## Global Search

I'm trying to wait for GtkListView with sections, but perhaps we can skip
that and add sections later. The search engine plumbing is there, along with
the dummy popover window.

I also want to add a secondary popover that allows us to animate in a display
of file content for the selected entry with highlighting.

## Find Other File

This supports F4 for switching between source/header/ui files and what not.
It used to build on the global search, and perhaps we would still want to
do that in some sort of way.

## Configuration Editing

This is still needed in the "Configure Project" window although I'm not sure
yet how we want to see that displayed. Should it be a list of additional pages?

Should they be sub-pages like languages are?

## Multiple Cursors

This needs to be ported to GTK 4 too, and merged into IdeSourceView again.
I don't like the code much and I'm not sure I want to maintain it, as it's a
giant pile of hacks that don't guarantee correctness.

## Beautifier

Ideally, just rewrite this plugin. It doesn't need the level of complication
it needed when it was originally written as Builder has less machinery then.

## Color Picker

This probably can use a lot of simplification from a rewrite as well.

## Command Bar

This will go away, and become part of the global search.

## Glade

This will go away, and instead use Drafting.

## GVls

Not sure what is needed here, but it should definitely be ported to
IdeLspService at minimum. We don't enable it in our builds anyway.

## Editor

This can be deleted once `editorui` is finished.

## Emacs

This can probably be deleted. We are unlikely to support emacs like we did
previously as it didn't get much usage and was often just broken.

## Devhelp

This needs to only use books and drop any sort of use of it's widgetry.

## Python Pack

Port the indenter to GtkSourceIndenter, try to merge upstream in GtkSourceView.

## C Pack

Also port the indenter, and merge it upstream as well. This one is nasty, and
could surely use a large rewrite.

## Vala Pack

Port the indenter, upstream to GSV

## Shell Command

The preferences is the part here that will need work as well as integrating
with the updated shortcut controller work.

## Sublime

Port to the new shortcuts engine. Can probably become a .json file descripting
all the keybindings in our new syntax.

## Sysprof

Port to use IdePage and get rid of the whole surface madness.




