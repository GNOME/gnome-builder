#!/usr/bin/env python3

import gi

from gi.repository import Ide
from gi.repository import GLib

_ERROR_REGEX = ("(?<filename>[a-zA-Z0-9\\-\\.\\/_]+.cs)" +
                "\\((?<line>\\d+),(?<column>\\d+)\\): " +
                "(?<level>[\\w\\s]+) " +
                "(?<code>CS[0-9]+): " +
                "(?<message>.*)")

class MonoPipelineAddin(Ide.Object, Ide.PipelineAddin):

    def do_load(self, pipeline):
        self.error_format = pipeline.add_error_format(_ERROR_REGEX, GLib.RegexCompileFlags.OPTIMIZE)

    def do_unload(self, pipeline):
        pipeline.remove_error_format(self.error_format)
