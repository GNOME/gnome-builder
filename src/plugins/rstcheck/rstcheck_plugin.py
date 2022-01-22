#!/usr/bin/env python3

import gi
import re
import threading
import time

from gi.repository import GLib, Gio, Ide


THRESHOLD_CHOICES = {
    'INFO': Ide.DiagnosticSeverity.NOTE,
    'WARNING': Ide.DiagnosticSeverity.WARNING,
    'ERROR': Ide.DiagnosticSeverity.ERROR,
    'SEVERE': Ide.DiagnosticSeverity.FATAL,
    'NONE': Ide.DiagnosticSeverity.NOTE,
}

class RstcheckDiagnosticProvider(Ide.Object, Ide.DiagnosticProvider):
    has_rstcheck = False

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.has_rstcheck = GLib.find_program_in_path('rstcheck')


    def create_launcher(self):
        context = self.get_context()
        srcdir = context.ref_workdir().get_path()
        launcher = None

        if context.has_project():
            build_manager = Ide.BuildManager.from_context(context)
            pipeline = build_manager.get_pipeline()
            if pipeline is not None:
                srcdir = pipeline.get_srcdir()
            runtime = pipeline.get_config().get_runtime()
            if runtime.contains_program_in_path('rstcheck'):
                launcher = runtime.create_launcher()

        if launcher is None:
            if not self.has_rstcheck:
                return None

            launcher = Ide.SubprocessLauncher.new(0)

        launcher.set_flags(Gio.SubprocessFlags.STDIN_PIPE | Gio.SubprocessFlags.STDERR_PIPE)
        launcher.set_cwd(srcdir)

        return launcher


    def do_diagnose_async(self, file, file_content, lang_id, cancellable, callback, user_data):
        task = Gio.Task.new(self, cancellable, callback)

        launcher = self.create_launcher()
        if launcher is None:
            task.return_error(Ide.NotSupportedError())
            return

        task.diagnostics_list = []

        threading.Thread(target=self.execute,
                         args=(task, launcher, file, file_content, cancellable),
                         name='rstcheck-thread').start()


    def do_diagnose_finish(self, result: Gio.Task) -> Ide.Diagnostics:
        if result.propagate_boolean():
            diagnostics = Ide.Diagnostics()
            for diagnostic in result.diagnostics_list:
                diagnostics.add(diagnostic)
            return diagnostics


    def execute(self, task, launcher, file, file_content, cancellable):
        try:
            # rstcheck reads from stdin when the input file name is '-'.
            launcher.push_args(('rstcheck', '-'))

            sub_process = launcher.spawn()
            stdin = file_content.get_data().decode('UTF-8')
            success, stdout, stderr = sub_process.communicate_utf8(stdin, cancellable)

            if stderr is None or len(stderr) < 1:
                task.return_boolean(True)
                return

            diagnostics = stderr.strip().split('\n')

            for diagnostic in diagnostics:
                # Example diagnostic text is:
                # '-:4: (WARNING/2) Inline strong start-string without end-string.'
                #
                # And this regex operation turns it into:
                # ['-', '4', 'WARNING', '2', 'Inline strong start-string without end-string.']
                diagnostic_text = re.split('\:([0-9]+)\:\s\(([A-Z]+)\/([0-9]{1})\)\s', diagnostic)

                file_name = diagnostic_text[0]
                on_line = int(diagnostic_text[1]) - 1
                warning_level = diagnostic_text[2]
                message = diagnostic_text[4]

                start = Ide.Location.new(file, on_line, 0)
                severity = THRESHOLD_CHOICES[warning_level]
                diagnostic = Ide.Diagnostic.new(severity, message, start)

                task.diagnostics_list.append(diagnostic)
        except GLib.Error as err:
            task.return_error(err)
        except Exception as e:
            task.return_error(GLib.Error('Failed to analyze reStructuredText content: {}'.format(e)))
        else:
            task.return_boolean(True)


