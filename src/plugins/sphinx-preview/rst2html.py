#!/usr/bin/env python3

# Copyright 2022 Christian Hergert <chergert@redhat.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-3.0-or-later

import docutils.core
import docutils.io
import os
import os.path
import sys

# This script is intended to be run as standard input to Python with
# the source content provided as file-descriptor 3. The result is
# written to standard output.

source = os.fdopen(3)
source_path = sys.argv[1]
dest_path = os.path.splitext(source_path)[0] + '.html'
docutils.core.publish_programmatically(docutils.io.FileInput,  # source_class
                                       source,                 # source
                                       source_path,            # source_path
                                       docutils.io.FileOutput, # destination_class
                                       sys.stdout,             # destination
                                       dest_path,              # destination_path
                                       None,                   # reader
                                       'standalone',           # reader_name
                                       None,                   # parser
                                       'restructuredtext',     # parser_name
                                       None,                   # writer
                                       'html',                 # writer_name
                                       None,                   # settings
                                       None,                   # settings_spec
                                       None,                   # settings_overrides
                                       None,                   # config_section
                                       True)                   # enable_exit_status
