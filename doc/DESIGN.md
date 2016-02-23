# LibIDE Design

LibIDE is a library that abstracts typical IDE operations.  It is meant to be
the base library behind Builder.  By separating this into its own static
library, we make things easier to unit test.  Or at least, that is the hope.

## Some Open Questions

 - How do we plumb editor settings in proper order?
   - Global Settings
   - Project Settings
   - Per-file modelines
 - How do we manage assets such as icons, settings, desktop and service files.
 - Does it make sense to add/remove/modify translations?
 - How do we want to plumb templates?
   Project templates, widget templates, etc.
   How does the vcs fit in with this.

## Objects

### IdeContext

`IdeContext` is the base object you will deal with in LibIDE. Think of it as a
library handle. Everything stems from an `IdeContext`.

Any object that is part of the `IdeContext` can register commands into the
context. The context has services, a project tree, build system, version
control system, unsaved files, search engine, and more.

In Builder, a context would be attached to a GbWorkbench.

### IdeBuildSystem

The `IdeBuildSystem` is responsible for loading projects and writing changes to the project back to disk.
It also registers commands that can be executed to perform transforms on the build system.
Such transforms might include adding a file.

### IdeProject

An `IdeProject` represents the project on disk.  Various subsystems collaborate
to build the project tree.  Files might come from the VCS. Build system targets
from the `IdeBuildSystem`.  Class names might come from various symbol
resolvers.  GtkWidget subclasses (for browsing widgets rather than files) might
come from another service.

The build system should update the tree when it performs transforms on the
underlying build system.  Such changes might include adding a file or target.

We need a good way to listen for changes on the tree so that UI can update
itself.

### IdeProjectItem

The `IdeProjectItem` represents an item in the `IdeProject` tree.  What the
project item contains is backend specific.  IDEs will need to know a bit about
the backend to render information in the IDE appropriately.

There will be some known subclasses that IDEs can traverse to find information
they want to know about.  Files in the project could be one (and loaded by the
VCS layer).  Targets could be another (and loaded by the build system).

### IdeLanguage

`IdeLanguage` represents a programming language, such as C, C++, or Python.  It
has some general utilities associated with it that can be used in IDEs.  One
such example is commenting a block of text.  `IdeLanguage` can be used to
retrieve an `IdeSymbolResolver`, `IdeRefactory` and others.

### IdeFile

An `IdeFile` is an abstraction of a file within the project.  It is partly a
convenience object so that we can map a file to a language as well as list
files in the VCS.  `IdeFile` also knows how to load a file from the VCS
backend.

### IdeBuffer and IdeBufferIter

`IdeBuffer` is an interface for passing around buffers that don't require
copying all of the text out of GtkTextBuffer or GtkTextIter slices.  It also
allows command handlers to work with a higher level structure than just a
string buffer.

### IdeIndenter

`IdeIndenter` is a class that can help you perform auto indentation in your
IDE.  Based on the cursor position in the file, it can suggest what the
indentation should be when various trigger keys are pressed.

### IdeRefactory

`IdeRefactory` represents a refactoring engine.  This can be retrieved for a
given language and the language can be retrieved from a given file.  The
refactory can expose available commands that can be executed.  Such an example
might be a "Extract Method" command.  Most of these will need to be programmed
into the IDE in a non-generic way.

### IdeVcs

An `IdeVcs` represents a version control system.  The version control system
can perform basic operations like creating a branch or snapshoting the
repository.  You can also add and remove files from the VCS, which should be
performed automatically when performing certain project transforms.

### IdeUnsavedFiles

The `IdeUnsavedFiles` object represents the collection of open buffers in the
IDE.  Many services such as Clang need access to these when determining code
completion and diagnostics.

The `IdeUnsavedFiles` class also abstracts the saving of modified buffers to
the drafts directory.  This allows us to maintain modified buffer state when
the user closes the window.

### IdeSearchEngine

The `IdeSearchEngine` manages search within the IDE.  It can have various
search providers that can resolve information.  Some search providers might
even store mined search information on disk for fast future lookup.  Having the
search engine live in LibIDE allows it fast access to project information,
files and refactory information like symbols.

### IdeSearchProvider

The `IdeSearchProvider` provides search results to the IdeSearchEngine.

### IdeSearchResult

The `IdeSearchResult` represents a search result.  It can be activated to jump
to a particular file or URI, line number, or other context specific
information.  IDEs will need to know about search result types to properly
route to the target information.

### IdeService

`IdeService` is a base service that lives inside an `IdeContext`.
These are used to implement singleton like features that aren't quite singletons.
They are per-context singletons.

This might be used by something such as a clang indexer to ensure only one
CXIndex exists per `IdeContext`.

### IdeDiagnoser

`IdeDiagnoser` provides access to diagnostics for a given file.  It will take
the unsaved file state into account via the `IdeUnsavedFiles` attached to the
`IdeContext`.

The `IdeDiagnoser` instance is retrieved from the `IdeLanguage` of the
`IdeFile`.  The `IdeFile` is passed to the `IdeDiagnoser` which will query the
`IdeClangService`.  The `IdeClangService` will use the unsaved file state in
`IdeUnsavedFiles` to pass state to the clang API.  Obviously, non-C languages
will have a service for their features as well (python, gjs, etc).

The `IdeDiagnoser` for C will be somewhat dependent on a build system since it
needs access to CFLAGS when communicating with Clang.

### IdeScript

An `IdeScript` is a user defined script that can run in the `IdeContext`.  It
is loaded using a language such as JavaScript to register new commands or tweak
settings as needed in the context.  For example,
~/.config/gnome-builder/scripts/*.js might be loaded into the context using
`IdeScript` to register custom commands.

### IdeDevice, IdeDeviceProvider and IdeDeviceManager

`IdeDevice` represents a device that can run software.

- It might be your local system, or even an xdg-app runtime on your local
  system.
- It might be an externally connected tablet or server.
- It might be a simulator in an application like Boxes.
- It might be a remote desktop, possibly alternate operating system.

The `IdeDeviceProvider` is responsible for discovering devices during device
settling.  Some devices might support connecting over TCP or Wi-Fi.

### IdeDebugger

Fetched via an `IdeDevice`.  Not all devices will support all debuggers.
Therefore we need to pass an IdeTarget to the device so it can determine if it
supports it.  pdb for python for example.

### IdeSymbolResolver

This is used to list symbols in a document, find a symbol by name, and fetch a
symbol at a give position in a file.  It should take the contexts unsaved files
into account.  A symbol resolver for C might access the same clang service that
is used for diagnostics, sharing their translation units.

### IdeDeployer

This is used to deploy a project to a device.  The local device is simpler in
that it is mostly just a `make install`.

Remove devices may be more complicated.  This will depend on both the build
system and target device to work properly.

Not all combinations will be supportable.

### IdeExecuter

This represents a strategy for executing a target on a particular device.
This is accessed from the device by passing the particular target.

### IdeProcess

This represents a subprocess that is executing a target. It could be local or
on a remote device. This is created by calling `ide_executer_execute()`.

### IdeTestSuite, IdeTestCase

Can be fetched from an `IdeBuildSystem`. They should be able to provide an
`IdeExecutable` that can be used to get an `IdeExecuter` by the `IdeDevice`.




## Questions and Answers

### How do I add a GSetting to my application?

After the IDE presents appropriate UI to the user, the IDE would activate the
"build-system.gsetting.add" command. Not all build systems may implement this
command, so it is important that the IDE check that the command is available
using `ide_context_has_command()`. You can also connect to the `::command-added`
and `::command-removed` signals which contain detailed quarks for the command
in question. This should make showing proper UI or enabling/disabling GActions
easier.

### How do I add a new executable target to my application?

Create a new IdeTarget subclass based with the information you desire.
You can check that the build system supports the target with
`ide_build_system_supports_target_type()` and providing the `GType`.
Then call `ide_build_system_add_target()`.

### How do I add a new C file to my shared library?

Create a new `IdeFile` using the parameters you want.
Then use `ide_target_add_file()` to add a file to a target, or another
interface method based on where you want to add the file.
`ide_build_system_add_file()` could be used to add the file starting at the
toplevel.

### What happens if I try to add a .vala file to a C-based shared library?

This should be okay if using autotools. So I imagine that based on the target,
it would accept or reject the item.

### How do I perform a trigger whenever a file is saved in the IDE?

In an IDE script, connect to the "file-saved" signal to perform an action.
The actual save is performed in the default handler, so mutating the content
can be done by hooking this signal. Using `G_CONNECT_AFTER` when connecting to
the signal will result in being called after the save has occurred.

`ide_context_emit_save_file()` and `ide_context_emit_file_saved()` should be
called by the IDE to trigger any scripts that desire handling the feature.

```
Context.connect('save-file', function(file, buffer) {
    data.replace('\r\n','\n');
});

Context.connect('file-saved', function(file, buffer) {
    console.log('file saved ' + file.get_name() + '\n');
});
```

### How do I get the list of targets found in a project?

`ide_build_system_list_targets()`

### How would a golang project be loaded?

Golang uses a particular directory layout, so a GolangBuildSystem would need to
be implemented. It would implement various commands for looking up targets and
building the project. However, no actual project file is used.

### How can we execute a target without a debugger?

```c
executer = ide_device_get_executer (device, target);
process = ide_executer_execute (executer);
```

### How would a user execute a particular, or all, test suites in a project.

Some build systems might implement an interface method to retrieve a list of
`IdeTestCase` or `IdeTestSuite`. It should then be possible to get an
`IdeExecutable` instance for the test that will result in the test being
executed. In some cases, this might run the test via a helper program such
as python.

### How would a user begin debugging a particular target executable on a remote device?

First we get a handle to the `IdeDebugger` for the `IdeDevice`.
We may need to specify something like an `IdeTarget` to be able to do so.

### How would a user get a list of symbols found in the current file?

First, the IDE would retrieve an `IdeSymbolResolver` for the given `IdeFile`.
Something like `ide_file_get_symbol_resolver()` seems like an obvious way to
go. Possibly via the IdeFile language provides a cleaner abstraction.

Then, use the resolvers interface methods to retrieve the list of symbols
within the file.

This does bring up the question of how should we load an IdeSymbolResolver for
a particular file. It might rely on both the `IdeBuildSystem` and the
`IdeLanguage` of the file.

Let's discuss the example of C. If the resolver for C can retrieve the CFLAGS
for a file using the `IdeBuildSystem`, it need to query the `IdeClangService`
to retrieve the list of symbols using the translation unit used with the file.
This means a concrete `IdeCSymbolResolver` concrete implementation.

An implementation for python, such as `IdePythonSymbolResolver` may be able to
work simply using the contents of the `IdeFile` since python provides an `AST`
method.

### How would a user get the symbol underneath the current cursor?

Similar to getting the list of symbols, the IDE would get a handle to a resolver
for the given `IdeFile`. Then it would use the particular method to get the
symbol underneath the line/column for the cursor.

### How would a user jump to the definition of a symbol?

Using the `IdeSymbolResolver` for the `IdeFile`, the IDE would call the method
to get the location of a particular symbol by name. Then it can open that file
using it's file loading subsystem.

### How would a user add a breakpoint to the active debugging process.

First, the IDE would get a handle to a debugger instance for the current
execution device. This would likely be performed via the `IdeDevice` that is
selected for execution. `ide_device_get_debugger()` passing the execution
target to be executed. Then the device implementation can determine if it has
a debugger suitable for that execution target.

Local might support pdb in addition to gdb. However, a tablet/phone might not.

### How would a user apply a fixit to a file?

First we access the the `IdeDiagnostics` for the `IdeFile`. Then we iterate
the available diagnostics that are available. Using one of them, the caller
can replace each file denoted by the diagnostic with the buffer modifications
provided.
