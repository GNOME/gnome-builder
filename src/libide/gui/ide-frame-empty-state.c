/* ide-frame-empty-state.c
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

#define G_LOG_DOMAIN "ide-frame-empty-state"

#include "config.h"

#include "ide-frame-empty-state.h"

struct _IdeFrameEmptyState
{
  GtkBin parent_instance;
};

G_DEFINE_TYPE (IdeFrameEmptyState, ide_frame_empty_state, GTK_TYPE_BIN)

static void
ide_frame_empty_state_class_init (IdeFrameEmptyStateClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-frame-empty-state.ui");
}

static void
ide_frame_empty_state_init (IdeFrameEmptyState *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
