#!/usr/bin/env python3

#
# html_preview_plugin.py
#
# Copyright (C) 2015 Christian Hergert <chris@dronelabs.com>
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

from gettext import gettext as _

from gi.repository import Builder
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import Ide

class HtmlPreviewAddin(GObject.Object, Builder.EditorView):
    def do_load(self, editor):
        self.menu_extension = Builder.MenuExtension(editor.get_menu())

        menu_item = Gio.MenuItem()
        menu_item.set_label(_("Preview as HTML"))
        menu_item.set_detailed_action('view.preview-as-html')
        self.menu_extension.append_menu_item(menu_item)

    def do_unload(self, editor):
        self.menu_extension.remove_items()

