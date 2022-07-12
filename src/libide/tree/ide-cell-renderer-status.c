/* ide-cell-renderer-status.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-cell-renderer-status"

#include "config.h"

#include "ide-cell-renderer-status.h"

#define VALUE_INIT_STATIC_STRING(name)         \
  {                                            \
    .g_type = G_TYPE_STRING,                   \
    .data = {                                  \
      { .v_pointer = (char *)name },           \
      { .v_uint = G_VALUE_NOCOPY_CONTENTS },   \
    },                                         \
  }

static GValue added = VALUE_INIT_STATIC_STRING ("builder-vcs-added-symbolic");
static GValue changed = VALUE_INIT_STATIC_STRING ("builder-vcs-changed-symbolic");
static GValue empty = VALUE_INIT_STATIC_STRING (NULL);

GtkCellRenderer *
ide_cell_renderer_status_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_PIXBUF,
                       "xpad", 3,
                       NULL);
}

void
ide_cell_renderer_status_set_flags (GtkCellRenderer  *renderer,
                                    IdeTreeNodeFlags  flags)
{
  if (flags & IDE_TREE_NODE_FLAGS_ADDED)
    g_object_set_property ((GObject *)renderer, "icon-name", &added);
  else if (flags & IDE_TREE_NODE_FLAGS_CHANGED)
    g_object_set_property ((GObject *)renderer, "icon-name", &changed);
  else
    g_object_set_property ((GObject *)renderer, "icon-name", &empty);
}
