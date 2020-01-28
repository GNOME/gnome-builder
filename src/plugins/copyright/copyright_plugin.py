#!/usr/bin/env python3

#
# copyright_plugin.py
#
# Copyright 2020 Christian Hergert <chergert@redhat.com>
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

from gi.repository import GLib, GObject, Gio
from gi.repository import Gtk
from gi.repository import Ide

import re
import time

_YEAR_REGEX = r1 = re.compile('([0-9]{4})')
_MAX_LINE = 100
_ = Ide.gettext

class CopyrightBufferAddin(Ide.Object, Ide.BufferAddin):
    settings = None

    def do_load(self, buffer):
        self.settings = Gio.Settings.new('org.gnome.builder.plugins.copyright')

    def do_unload(self, buffer):
        self.settings = None

    def do_save_file(self, buffer, file):
        """
        If there is a copyright block within the file and we find the users
        name within it, we might want to update the year to include the
        current year.
        """
        # Stop if we aren't enabled
        if not self.settings.get_boolean('update-on-save'):
            return

        # Stop if we can't discover the user's real name
        name = GLib.get_real_name()
        if name == 'Unknown':
            return

        year = time.strftime('%Y')
        iter = buffer.get_start_iter()
        limit = buffer.get_iter_at_line(_MAX_LINE)

        while True:
            # Stop if we've past our limit
            if iter.compare(limit) >= 0:
                break

            # Stop If we don't find the user's full name
            ret = iter.forward_search(name, Gtk.TextSearchFlags.TEXT_ONLY, limit)
            if not ret:
                break

            # Get the whole line text
            match_begin, match_end = ret
            match_begin.set_line_offset(0)
            if not match_end.ends_line():
                match_end.forward_to_line_end()
            text = match_begin.get_slice(match_end)

            # Split based on 4-digit years
            parts = _YEAR_REGEX.split(text)

            # Ignore if this year is already represented
            if year in parts:
                break

            # If we have at least 2 years, we can update them
            if len(parts) >= 2:
                if '-' in parts:
                    parts[parts.index('-')+1] = year
                else:
                    parts.insert(2, '-')
                    parts.insert(3, year)

                text = ''.join(parts)

                # Replace the line text in a single undo action
                buffer.begin_user_action()
                buffer.delete(match_begin, match_end)
                buffer.insert(match_begin, text)
                buffer.end_user_action()

                break

            # Move to the end of the line
            iter = match_end

class CopyrightPreferencesAddin(GObject.Object, Ide.PreferencesAddin):
    def do_load(self, prefs):
        self.update_on_save = prefs.add_switch(
                "editor", "general",
                "org.gnome.builder.plugins.copyright",
                "update-on-save",
                None,
                "false",
                _("Update Copyright"),
                _("When saving a file Builder will automatically update copyright information for you"),
                # translators: these are keywords used to search for preferences
                _("update copyright save"),
                10)

    def do_unload(self, prefs):
        prefs.remove_id(self.update_on_save)
