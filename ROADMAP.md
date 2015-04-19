# Builder Roadmap

## GNOME Builder 3.16.3

 * General bugfixes

## GNOME Builder 3.18.0

### Templates

 * Templates will be implemented as GResource bundles
 * foo.template or similar template overview file

#### Project Templates

##### GNOME 3 Application

 * Create GNOME 3 style application
   - Search pattern
   - Browser pattern
   - etc

 * Choose language for template
   - Python
   - C
   - Vala
   - JavaScript

 * Application creates ELF
   - GJS has main(), loads JS from embedded GResource
   - Python has main(), hooks Python loading from GResources
   - Vala, C are done as normal

#### Other Templates

##### GObject

 * Create new GObject (header, source, add to Makefile.am)
 * Specify property names, signals, and method stubs

##### GtkWidget

 * Create new GtkWidget (header, source, .ui, Makefile.am)


### Navigation

 * Improve navigation stack
   - Drop "current item"

### Version Control

#### Search for Project from Github, git.gnome.org

 * Cache credentials (or GOA), show list of repositories visible to user.

#### Snapshot Repository

 * Easily snapshot current working directory
 * Store snapshot in a branch
 * View snapshot history
 * Select snapshot to rollback to

#### Stage Commit

 * Custom view for staging a commit


## Snippets

 * Snippets editor in preferences
 * Dynamic language support for snippet language


## Keybindings

 * Keybindings editor for preferences
 * Keybindings overlay for workbench


## UI Designer

 * Basic glade integration
 * Template support (requires upstream work)


## Project Management

### Autotools

 * Add target
   - library (c, vala)
   - executable (c, vala, python, js)
 * Add desktop entry
 * Add gsetting
 * Add dbus service
 * Add dependency
   - pkg-config
   - program
   - library
   - header


## External Build Tools

 * Set custom build step, execute with menu item or accelerator.


## Build Support

 * Build output panel
 * Build/Rebuild buttons


## Symbol Browser

 * Project wide symbol browser panel?


## Project Tree

 * Browse "Sources" and "Headers", sibling to "Files"
   - Hide if there are zero children
 * Browse "Targets"

## Search and Replace

 * External Dialog or integrated into GbEditorView?
 * Search options for top-right search widget


## Global Search

 * Symbol Indexing and Searching
 * grep (-A -B)

