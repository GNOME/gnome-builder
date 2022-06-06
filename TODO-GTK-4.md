# GTK 4 Notes

This document is meant to help keep track of things that we will need to come back to as part of the GTK 4 port.

## IdeTree

 * DnD drop targets need work, as they aren't being handled yet. This will be easier with plugins to test it.
 * The cell renderer uses cairo drawing. Instead we should use icons for the Git states so we get GDK texture caching.

## Shortcuts

We have a shortcut manager now, but we probably need a way to get a shortcut
trigger from an action name so that we can show accel previews in various
menu models. We might also want a way to get description/etc for a shortcut
search with the upcoming global search.

Plugins need to be ported to add a gtk/keybindings.json file with their
given trigger/action mappings.

Some objects were using helpers that exported action signals as actions and
those need to be updated to use proper actions. Many just need to be converted
as GTK itself provides actions now.

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
