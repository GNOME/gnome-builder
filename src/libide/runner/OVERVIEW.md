# Running Projects

Running a project target in Builder needs to accomidate a few difficult
to plumb components. For example, Builder supports runtimes which might
be different than the current host (such as org.gnome.Platform 3.22).
Additionally, we might need to attach a debugger. The build system might
also need to perform an installation of the application bits into a
runtime so that the project can run as its "installed state".

All of these complexities results in project execution being abstracted
into an “IdeRunner” object and series of “IdeRunnerAddin” extensions.
This allows the various plugins involved (build system, runtime, possible
debugger or profilers) to hook in to various stages and modify things as
necessary.

# IdeRunner

This is our core runner structure. It manages some basic process execution
stuff, but most things get tweaked and configured by the addins.

The process is launched by the IdeSubprocessLauncher created by the configured
runtime. However, addins can tweak things as necessary.

## IdeRunnerAddin

Plugins should implement this interface so that they can modify the runner
as necessary. This should be used for custom extensoin points that are always
needed, not to add integration points like debugger, profiler, etc.

## Debugger Integration

Debuggers may need to hook into the runtime pid namespace (and possibly mount
namespace to bring along tooling). This could also mean starting the process as
an inferior of something like `gdb`. In the flatpak scenario, this could be

  flatpak reaper → gdb → inferior

Additionally, the debugger will need to be able to IPC with the gdb instance
inside the runtime.

While we don't have support for this yet, I think the design we can use is that
the `IdeRunner` can have API to bring in "sidecars" to the runtime which
contain the debugging tooling. For the local system, this would be a
passthrough. For something like flatpak, it would need to enter the namespace
and add a mountpoint for the tooling. (This is probably something that needs to
be implemented in bubblewrap to some degree).

When the debugger sets up the runner, it will need to be able to modify argv
similar to our IdeSubprocessLauncher. This will allow it to add gdb to the
prefix of the command.

Getting a runner will be somewhat consistent with:

  BuildSystem = context.get_build_system()
  ConfigManager = context.get_configuration_manager()

  config = ConfigManager.get_current()
  runtime = config.get_runtime()
  target = BuildSystem.get_default_run_command(config)
  runner = runtime.create_runner(config, target)

    -> the build system, at this point, may have added a prehook via
       the runner addin that will install the project before it can
       be run.

And the debugger might do something like:

  runner.prepend_argv('gdbserver')
  runner.prepend_argv('--once')
  runner.prepend_argv('--args')

When runner.run() is called, the implementation might choose to prepend
additional argv parameters. This allows gdb to simply prepend the command
line, but still have flatpak spawn the container process (bubblewrap).

## Profiling Integration

A profiler (such as sysprof) will need to know the process identifier of the
spawned process (and possibly children processes). This is somewhat similar
to debugging except that it isn't strictly necessary that the process stops
before reaching `main()`.

To synchronize on startup, sysprof might spawn a helper process that
communicates with the listener process, and then execs the child command.

So it might need to do something like:

  runner.prepend_argv('sysprof-spawner')


## RunHandlers

Because we want one runner at a time, and different plugins might implement
different runners (debugger, profiler, basic run support), we register
a run handler.

When ide_run_manager_run_async() is called, the run handler will be called
and it can adjust things as necessary.


