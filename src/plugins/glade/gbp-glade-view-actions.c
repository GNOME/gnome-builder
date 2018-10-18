/* gbp-glade-view-actions.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#define G_LOG_DOMAIN "gbp-glade-view-actions"

#include <glib/gi18n.h>

#include "gbp-glade-private.h"

static void
gbp_glade_view_action_save (GSimpleAction *action,
                            GVariant      *param,
                            gpointer       user_data)
{
  GbpGladeView *self = user_data;
  g_autoptr(GError) error = NULL;
  const gchar *path;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_GLADE_VIEW (self));

  if (self->file == NULL || !(path = g_file_peek_path (self->file)))
    {
      g_warning ("GbpGladeView is missing a file");
      return;
    }

  if (!glade_project_save (self->project, path, &error))
    /* translators: %s is replaced with the specific error message */
    ide_widget_warning (self, _("Failed to save glade document: %s"), error->message);
}

static GActionEntry actions[] = {
  { "save", gbp_glade_view_action_save },
};

void
_gbp_glade_view_init_actions (GbpGladeView *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  g_assert (GBP_IS_GLADE_VIEW (self));

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "glade-view",
                                  G_ACTION_GROUP (group));
}
