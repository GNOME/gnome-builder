# Journal

This is my notes as i go, to see if we can recolect on anything useful.

## Day 1 — Starting a new (sub) project

To avoid having to refactor large bits of Builder, I've started a new project
tree where I can just bring things over incrementally as I clean them up and
port them to the new design.

The big thing I'm trying to do here is:

 * Break things up into a series of smaller libraries (libide-core, libide-gui,
   libide-threading, etc).
 * Rename lots of objects to more closely represent what they do, which wasn't
   as clear when they were initially designed. Sometimes things get used
   differently than their intention.
 * Experiment with a new design for IdeObject and IdeContext that simplifies
   reference counting, cycles, and other hard to detect issues.
 * Create a very clear addin story in terms of what interfaces you need to
   extend for your particular goal.
 * Create an abstract enough base-layer that we can create similar projects
   such as a dedicated $EDITOR (with minimal projects interaction) and something
   like the purple-egg prototype.
 * Some sort of multi-monitor story.

That is quite ambitious for a branch, but I think they are all somewhat related
in terms of getting this done cleanly.

It might also help us figure out how more correctly support an ABI story in the
future, if that becomes important.

Another thing on my mind, is how can this help us in a Gtk4 story. There will
be lots of porting there, so what can we clean-up now?

I've done lots of drawing in my engineering journal to ensure that I have a
grasp of the windowing objects, and how we want to name them succinctly.

Anyway, that is what I've been doing mostly today. I've gotten a bit of
libide-core created, lots of layout stuff extracted into libide-gui, and a
threading library built (subprocess, tasks, thread pool, environ, etc).


## Day 2 — More work on greeter, library cleanup

Lots of work on cleaning up greeter apis, which were previously quite shitty.

Realized that what I want a "workbench" to be, is basically a GtkWindowGroup.
Well that simplifies a bunch since we can subclass it.

I'm starting to get the header bar refactored into something re-usable, now that
we'll likely have multiple types of workbenches (Primary, Secondary, Greeter,
Dedicated Editor (gedit), and Terminal (purple-egg).

I did another round of cleanup for how I want library headers to work. It's
starting to look much better.

I moved PTY interceptor to libide-io, because it allows us to avoid having
the libide-terminal (having gtk widgets) a dependency of the build library
later on.

I started on the projects library. I also am re-using project-info object
for opening projects, since it allows us to describe more about what we
are trying to open (like a VCS uri).

One interesting thing is the idea of moving away from the omnibar design as it
is now, where it knows what to look for to display (like the build pipeline
type stuff). We can instead create an IdeMessage and get rid of IdePausable
in favor of that. Then the pipeline can request a message and update it during
the process of the build. That doesn't solve the content of the popover, but
we can deal with that in later API.

An insight while working on IdeObject is that we can sort of copy how things
are done in Gtk (but without floating references). I'm not quite sure yet
how to ensure you can't add new objects to the tree during destroy though.
That could be an interesting race condition to solve.

If the objects have cancellables, and those are cancelled during ::destroy,
then the tree will just cleanup work as it is no longer necessary.


## Day 3 — More work on objects

Spent most of the day on a few iterations of the object stuff. It seems
like I started to let it get out of hand with the requirements.

So I reeled it back in. IdeObject is a main thread only object, that is
capabile of interacting with other threads. So workers (like pipelines, etc)
can still use threads, just do the tree stuff on the main thread only.

Also, IdeObject::destroy seems to work, and since it is only valid to
dispose them from main thread, i think we get the effect we want.

It does mean that consumers need to use IdeTask though.


## Day 4 — OmniBar, Message, HeaderBar, etc

I think IdeContext can exist in an "unloaded" form and be created with the
IdeContext. It just wont have many children other than transfer-manager,
messages, etc.

I started working on the omnibar a bit, and seeing how it will fit into the
new header bar abstractions. To keep the OmniBar from having to know about
the buildsystem/etc, we can make it use addins and some API to extend it.

I also worked a bit on making IdeMessage support the features we want. I also
drew some mockups for how IdeMessage would be visualized in the new OmniBar.

I'm thinking about how to rotate data in the omnibar, and how addins get their
messages in. Basically, I think we want to just have the omnibar discover the
IdeMessages object as a root IdeObject child. Then treat that as a GListModel
that creates widgets for the children in a GtkStack (with transitions).

I think we can use IdeNotification to replace IdePausable, IdeTransfer display,
and omnibar build status messages. We'll still have IdeTransfer/
IdeTransferManager, but they can be child objects of the IdeContext (which is
unloaded for preferences at startup, but can still exist).

The display of messages will be split based on if it has progress. Things with
progress go under the progress button. Thinks without go under the build
popover. But all are represented by an IdeMessage.

## Day 5 — OmniBar, Messages

Starting again on this work after some fresh thought.

I think IdeMessage is starting to look a lot like a Notification API, and we
should probably use that as our metaphore (renaming to IdeNotification). That
allows for actions (buttons), icons, status, progress, etc. We'll just have to
be thread-safe so it can be used from threads to send status. (Like during git
clone events).

Goals for today:

 x IdeMessage renamed to IdeNotification
 o IdeNotificationView -> GtkWidget to show IdeNotification in omnibar stack
 - IdeNotificationListBoxRow -> GtkWidget to show full notification, either in
   the omnibar popover, or in the transfers popover. Those with progress will
   always be in the transfers (progress?) button.
 - IdeProgressButton -> Rename from Transfers button, use Notification API
 - IdeOmniBarAddin (allow addins to extend omnibar)

I got some more of the workbench/workspace setup, and initial context setup.
What I like so far is that all workbenches will have an IdeContext, just that
a project may not yet be loaded into that context. That can simplify a lot of
our inconsistencies between the two states.

I want to avoid using a Popover for the omnibar popdown, unless it is also
transient like we did for the hoverprovider. It's just too annoying to have
to click through the entry box. I'd like it to be a quick hover, and then
the window is displayed, possibly even wrapping around the omnibar.

## Day 6 — OmniBar, Continued

We got a lot of stuff done yesterday, but today I want to push through and
try to finish more of it. The notification stuff is working well. I want to
get the progress button stuff done, and the listboxrows for the popover.

We also should create the omnibar addin, since that is pretty easy.


## Day 7 — Refinement, meson

Spent some time today trying to refine the notification work so that we can
move on with the workbench/workspaces.

I started on some new IdeObject helpers to make compat with the old libide
design easier in a few places.

I got search ported over, albeit we're going to have to get rid of the
source location stuff and switch to just an "activate" signal. I'd also
like to be able to drop DzlSuggestion (so we can drop dazzle from that
library too).

I'm also starting to switch to meson, because my crappy makefile was too
slow and it will help me think about the dependencies of each library easier.

We're definitely in a slog here, but it's going to be worth it.


## Day 8 — Loading, Initialization, etc

The next big challenge to handle is how do we start loading the initial
system components that are not available in libide-core.

We can do some of that work in parallel, for example discovering the
build system while we also discover the version control system.

Some components will probably need to wait for another object to appear
on the tree before they can do anything. For example, some things may not
be practical until the IdeVcs has attached itself to the context.

Furthermore, an IdeWorkspace window might want to load additional content
once there is a known git vcs loaded. We probably need a way to watch for
object attachments to simplify that.

Possibly something like:

```
ide_object_wait_for_child_async (parent, IDE_TYPE_VCS, NULL, child_cb, foo);
```

Now that we have IdeContextAddin, we could allow any of the VCS systems to
attach themselves to the VCS. For this to work, all addins would need to
have an async vfunc pair to load the project.

```
  void (*load_project_async) (IdeContextAddin     *context,
                              GFile               *file,
                              GVariant            *hints,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data);
```

--

I'm pretty happy with how things turned out, but I decided to use
IdeWorkbenchAddin for everything instead of IdeContext, so that we can keep
more objects out of libide-core. I want to keep that library small.

I also added lots more vfuncs to WorkbenchAddin so that it could handle the
project being open (when plugin is delayed on opening).

Anyway, I got some basic workbench/workspace mechanics working, project loading
and unloading, and plumbing to open files. Workbench addins can also be
notified of workspace creation/etc.

## Day 9 — Workspace tracking, surfaces

Today I want to focus on ensuring we can handle "surfaces" well. Previously,
that was called "Perspective" but that never really fit right to me. A surface
is sort of like an area of the workspace that you can do something on. A
worksurface seemed a bit long in the tooth.

We also need to track the focus chain of lots of things (windows, frames, pages)
so that we can "do the right thing" when actions occur for the user. For example,
if the user focuses the search box from a secondary window, they probably still
want the new page to display in the secondary window.

-----

I ended up doing a bunch more work on libide-code beyond basic surface stuff,
so that we can start importing more code from master. Some notable changes

 - IdeFixit -> IdeTextEdit will allow us to kill both IdeFixit and IdeProjectEdit
   to some degree.
 - Make all the diagnostic/range/location/fixit IdeObjects. I'm not sure how
   we'll do object ownership though (for the object tree). I guess they can
   be floating, but that doesn't help when tracking down memory issues.

-----

Another thing I'm looking at doing is making all our little libraries statically
linked into the gnome-builder exec. That is much better given our "private ABI"
that we currently have. We can still export our API symbols just fine.


Question: What is the best way to keep libraries around after static linking
without relying on peas? We can just do _foo_init() for libide base libs.

## Day 10 — Builds Questions, Static Linking

I want to focus today on figuring out the more pressing build questions that
will give us the outcome we desire. My biggest desire with the new design is
that we can get to a single executable, `gnome-builder`. All of the workspace
forms (ide, editor, terminal) should still route through this executable.

That means we need to use static linking for all of the libraries, and the
GCC/LD option `--require-defined=_foo_register_types` for the plugins that
will be discovered/registered by the libpeas engine. Also, since we need to
export symbols to the plugins, we need to be -export-dynamic and ensure that
we are only providing the symbols we care to export.

Some symbols are used across the static linking boundary (like initing the
themes static lib) but do not need to be exported. That works fine by just
omitting our `_IDE_EXTERN` or `IDE_AVAILABLE_IN_*` macros. But when combined
with GResources, we will lose our automatic constructor loading of the
resources.

What slightly complicates things is that we have peas `.plugin` files that
need to show up automatically, or libpeas wont know to discover their types
init function. So we need 2 functions in those libraries. One is the peas
type registration (`_foo_register_types()`), and the other to ensure that
the base resources are registered such as calling:

```
g_resources_register (foo_get_resource ());
```

Calling the `foo_get_resource()` should be enought to ensure the link, so
if we do that we can avoid a secondary function and have the `get_resource`
disappear after the link (non-exported symbol).


-----

A marathon of a day, but I managed to get libide-code mostly ported over
and compiling. Some API had to change, but that was expected.

I still need to figure out how I want the buffer-change-monitor to be
created so libide-code doesn't need to depend on libide-vcs. It would
be nice if we could use a peas extension for that, and let the plugins
check the current VCS to see if they care.

The big API changes include some stuff like:

 - Using GObject for diagnostic(s), location, ranges
 - getting rid of `ide_context_get_*()` pattern and inverting
   it to `ide_*_from_context()`.

I think the next big thing is to try to get an editor view visible.


