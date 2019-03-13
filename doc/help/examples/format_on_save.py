#!/usr/bin/env python3

from gi.repository import GObject
from gi.repository import Gio
from gi.repository import Ide

_NEEDS_BUILD_RUNTIME = False

class MyBufferAddin(GObject.Object, Ide.BufferAddin):

    def do_save_file(self, buffer: Ide.Buffer, file: Gio.File):

        # Ignore everything if this isn't C code.
        # The language identifier comes from gtksourceview *.lang files
        lang = buffer.get_language()
        if lang is None or lang.get_id() not in ('c', 'chdr'):
            return

        # If you need to run the program in the build environment, you might
        # need to do something like:
        if _NEEDS_BUILD_RUNTIME:
            context = buffer.ref_context()
            runtime = context.get_build_manager().get_pipeline().get_runtime()
            launcher = runtime.create_launcher()
        else:
            launcher = Ide.SubprocessLauncher.new(0)

        # Make sure we get stdin/stdout for communication
        launcher.set_flags(Gio.SubprocessFlags.STDIN_PIPE |
                           Gio.SubprocessFlags.STDOUT_PIPE)

        # Setup our cmdline arguments
        launcher.push_argv('indent')

        # If your target program is installed on the host (and not bundled
        # or found in the build environment runtime) you might need to set
        # this so the program runs on the physical host, breaking out of
        # the sandbox (yes, we can do that).
        launcher.set_run_on_host(True)

        # Launch the process
        subprocess = launcher.spawn()

        # Pipe the buffer contents to the indent process.
        # communicate_utf8() will raise if it fails.
        begin, end = buffer.get_bounds()
        text = buffer.get_text(begin, end, True)
        ret, stdout, stderr = subprocess.communicate_utf8(text, None)

        # Now write the new contents back
        buffer.set_text(stdout, len(stdout))

        # TODO: You might want to save the location of the insertion
        #       cursor and restore it after replacing the contents.
        #       Otherwise, the cursor will jump to the end of the file.

