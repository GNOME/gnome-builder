#################################
Subprocesses and Pseudo Terminals
#################################


Creating Subprocesses
=====================

Builder provides a powerful abstraction for creating subprocesses.
``Ide.Subprocess`` allows you to setup and modify how a processes should be launched without the burden of how to launch the subprocess.
This means that Builder can use different strategies based on the host system, subprocess requirements, and plugins that may need to modify the program arguments.

When Builder is not running in a sandbox, it can generally execute subprocesses the normal way using fork and exec.
However, if Builder is sandboxed, it may need to run the subprocess on the host rather than inside the sandbox.
To ensure your subprocess is run on the host, use ``Ide.Subprocess.set_run_on_host()``.

.. note:: You can only run programs on the host that are already installed.
          Use ``which program-name`` to determine if the process is available on the host.

If you are integrating an external tool, such as Valgrind, you might need to inject arguments into the argument array.
For the Valgrind case, ``Ide.Subprocess.prepend_argv()`` of ``"valgrind"`` would be appropriate.

Some runtime plugins may need to modify the argument array even further.
For example, the Flatpak plugin will require that all commands start with ``flatpak build ...`` so that the commands are never run on the host system, but instead inside the runtime.
Plugins that require this should inject their additional arguments from the ``Ide.SubprocessLauncher.spawn()`` virtual-method so that plugins do not get confused about the placement of arguments.


.. code-block:: py

   from gi.repository import GLib
   from gi.repository import Ide

   # You may want access to stdin/stdout/stderr. If so, ensure you specify
   # the appropriate Gio.SubprocessFlags for your subprocess.
   launcher = Ide.SubprocessLauncher.new(Gio.SubprocessFlags.STDOUT_PIPE |
                                         Gio.SubprocessFlags.STDERR_PIPE |
                                         Gio.SubprocessFlags.STDIN_PIPE)

   # If you need to specify where to launch the process. The default
   # is the home directory.
   launcher.set_cwd(os.path.join(GLib.get_home_dir(), 'Projects'))

   # Push some arguments onto argv
   launcher.push_argv('which')
   launcher.push_argv('ls')

   # Set some environment variables
   launcher.setenv('LANG', 'C', True)

   # Spawn the process. If you pass in a Gio.Cancellable, you can kill the
   # subprocess by calling Gio.Cancellable.cancel().
   subprocess = launcher.spawn(None)

   # We need to wait for the child to complete. If you want to read the
   # output of the subprocess, see Ide.Subprocess.communicate_utf8().
   # wait_check() will ensure the return value is zero. If you do not
   # care about the return value, just use wait().
   try:
       subprocess.wait_check(None)
   except Exception as ex:
       print(repr(ex))

    # May Ide.Subprocess API have async variants. Consider using them to
    # avoid needlessly blocking threads.


Supervising Subprocesses
========================

There are times where you might want to respawn a process in case it exits prematurely.
Builder provides the ``Ide.SubprocessSupervisor`` abstraction to simplify this for you.

The ``Ide.SubprocessSupervisor`` has a simple API.
Just attach your ``Ide.SubprocessLauncher`` using ``Ide.SubprocessSupervisor.set_launcher()`` and call ``Ide.SubprocessSupervisor.start()``.

If the subprocess begins flapping (exiting immediately after spawning) some delay will be added to slow things down.

To stop the subprocess, use ``Ide.SubprocessSupervisor.stop()``.

If you need access to the subprocess, you can access it either via the ``Ide.SubprocessSupervisor.get_subprocess()`` method or by connecting to the ``Ide.SubprocessSupervisor::spawned()`` signal.

.. code-block:: py

   def on_subprocess_spawned(supervisor, subprocess):
       print("Spawned process " + subprocess.get_identifier())

   launcher = create_launcher()

   supervisor = Ide.SubprocessSupervisor()
   supervisor.set_launcher(launcher)
   supervisor.connect('spawned', on_subprocess_spawned)
   supervisor.start()


Pseudo Terminals
================

Pseudo terminals are tricky business.
In general, if you need access to a PTY, use the VTE library like Builder's terminal plugin.
For an example of how to setup the PTY, we use a flow like this.

.. code-block:: c

   // This code does little to no error checking.
   // Your code should be more careful.

   // First create our PTY master
   VtePty *pty = vte_terminal_pty_new_sync (terminal,
                                            VTE_PTY_DEFAULT | VTE_PTY_NO_LASTLOG | VTE_PTY_NO_UTMP | VTE_PTY_NO_WTMP,
                                            NULL, &error);

   // Now go through the PTY slave setup
   int master_fd = vte_pty_get_fd (pty);
   
   assert (grantpt (master_fd) != 0);
   assert (unlockpt (master_fd) != 0);

   // Get the path to the PTY slave
   char name[PATH_MAX];
   assert (ptsname_r (master_fd, name, sizeof name - 1) != 0);
   name [sizeof name - 1] = '\0';

   // Open the PTY slave
   int slave_fd = open (name, O_RDWR | O_CLOEXEC);

   // Now, when spawning a process, you can set stdin/stdout/stderr to the FD
   // of the slave. We use dup() because the callee takes ownership.
   ide_subprocess_launcher_take_stdin_fd (launcher, dup (slave_fd));
   ide_subprocess_launcher_take_stdout_fd (launcher, dup (slave_fd));
   ide_subprocess_launcher_take_stderr_fd (launcher, dup (slave_fd));
   close (slave_fd);

When launching the subprocess with Builder, it will detect that ``stdin``, ``stdout``, or ``stderr`` are pseudo terminals and perform the proper ``ioctl()`` setup for you.
This allows for the PTY to cross the sandbox boundary to the host, ensuring that you may have a host-based shell with a PTY from within the sandbox.

