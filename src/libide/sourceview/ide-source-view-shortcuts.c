/* ide-source-view-shortcuts.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-source-view-shortcuts"

#include "config.h"

#include <dazzle.h>

#include "ide-source-view.h"
#include "ide-source-view-private.h"

void
_ide_source_view_init_shortcuts (IdeSourceView *self)
{
  DzlShortcutController *controller;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));

  dzl_shortcut_controller_add_command_signal (controller,
                                              "org.gnome.builder.sourceview.reset",
                                              "Escape",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              "reset", 0);
}
