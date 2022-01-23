#!/usr/bin/env python3

import gi
import re

from gi.repository import GLib, Gio, Ide

THRESHOLD_CHOICES = {
    'INFO': Ide.DiagnosticSeverity.NOTE,
    'WARNING': Ide.DiagnosticSeverity.WARNING,
    'ERROR': Ide.DiagnosticSeverity.ERROR,
    'SEVERE': Ide.DiagnosticSeverity.FATAL,
    'NONE': Ide.DiagnosticSeverity.NOTE,
}

class RstcheckDiagnosticProvider(Ide.DiagnosticTool):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.set_program_name('rstcheck')

    def do_configure_launcher(self, launcher):
        # rstcheck - signifies that stdin will be used
        launcher.push_argv('-')

    def do_populate_diagnostics(self, diagnostics, file, stdout, stderr):
        try:
            if stderr is None or len(stderr) < 1:
                return

            for line in stderr.strip().split('\n'):
                # Example diagnostic text is:
                # '-:4: (WARNING/2) Inline strong start-string without end-string.'
                #
                # And this regex operation turns it into:
                # ['-', '4', 'WARNING', '2', 'Inline strong start-string without end-string.']
                text = re.split('\:([0-9]+)\:\s\(([A-Z]+)\/([0-9]{1})\)\s', line)

                file_name = text[0]
                on_line = int(text[1]) - 1
                warning_level = text[2]
                message = text[4]

                start = Ide.Location.new(file, on_line, 0)
                severity = THRESHOLD_CHOICES[warning_level]
                diagnostic = Ide.Diagnostic.new(severity, message, start)

                diagnostics.add(diagnostic)
        except Exception as e:
            Ide.warning('Failed to analyze reStructuredText content: {}'.format(e))
