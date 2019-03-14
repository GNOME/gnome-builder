/* gbp-glade-page-actions.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-glade-page-actions"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-glade-private.h"

static void
gbp_glade_page_action_save (GSimpleAction *action,
                            GVariant      *param,
                            gpointer       user_data)
{
  GbpGladePage *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_GLADE_PAGE (self));

  if (!_gbp_glade_page_save (self, &error))
    /* translators: %s is replaced with the specific error message */
    g_warning (_("Failed to save glade document: %s"), error->message);
}

static void
gbp_glade_page_action_preview (GSimpleAction *action,
                               GVariant      *param,
                               gpointer       user_data)
{
  GbpGladePage *self = user_data;
  GladeProject *project;
  GList *toplevels;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_GLADE_PAGE (self));

  project = gbp_glade_page_get_project (self);
  toplevels = glade_project_toplevels (project);

  /* Just preview the first toplevel. To preview others, they need to
   * right-click to get the context menu.
   */
  for (const GList *iter = toplevels; iter != NULL; iter = iter->next)
    {
      if (GTK_IS_WIDGET (iter->data))
        {
          GtkWidget *widget = iter->data;
          GladeWidget *glade;

          g_assert (GTK_IS_WIDGET (widget));

          glade = glade_widget_get_from_gobject (widget);
          g_assert (GLADE_IS_WIDGET (glade));

          glade_project_preview (project, glade);

          break;
        }
    }
}

static void
gbp_glade_page_action_pointer_mode (GSimpleAction *action,
                                    GVariant      *param,
                                    gpointer       user_data)
{
  GbpGladePage *self = user_data;
  g_autoptr(GEnumClass) klass = NULL;
  GladeProject *project;
  const gchar *nick;
  GEnumValue *value;
  GType type;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));
  g_assert (GBP_IS_GLADE_PAGE (self));

  project = gbp_glade_page_get_project (self);
  nick = g_variant_get_string (param, NULL);

  /* No GType to lookup from public API yet */
  type = g_type_from_name ("GladePointerMode");
  klass = g_type_class_ref (type);
  value = g_enum_get_value_by_nick (klass, nick);

  if (value != NULL)
    glade_project_set_pointer_mode (project, value->value);
}

static void
gbp_glade_page_action_paste (GSimpleAction *action,
                             GVariant      *param,
                             gpointer       user_data)
{
  GbpGladePage *self = user_data;
  GtkWidget *placeholder;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_GLADE_PAGE (self));

  placeholder = glade_util_get_placeholder_from_pointer (GTK_CONTAINER (self));
  glade_project_command_paste (self->project, placeholder ? GLADE_PLACEHOLDER (placeholder) : NULL);
}

#define WRAP_PROJECT_ACTION(name, func)                 \
static void                                             \
gbp_glade_page_action_##name (GSimpleAction *action,    \
                              GVariant      *param,     \
                              gpointer       user_data) \
{                                                       \
  GbpGladePage *self = user_data;                       \
                                                        \
  g_assert (G_IS_SIMPLE_ACTION (action));               \
  g_assert (GBP_IS_GLADE_PAGE (self));                  \
                                                        \
  glade_project_##func (self->project);                 \
}

WRAP_PROJECT_ACTION (cut, command_cut)
WRAP_PROJECT_ACTION (copy, copy_selection)
WRAP_PROJECT_ACTION (delete, command_delete)
WRAP_PROJECT_ACTION (redo, redo)
WRAP_PROJECT_ACTION (undo, undo)

static GActionEntry actions[] = {
  { "cut", gbp_glade_page_action_cut },
  { "copy", gbp_glade_page_action_copy },
  { "paste", gbp_glade_page_action_paste },
  { "delete", gbp_glade_page_action_delete },
  { "redo", gbp_glade_page_action_redo },
  { "undo", gbp_glade_page_action_undo },
  { "save", gbp_glade_page_action_save },
  { "preview", gbp_glade_page_action_preview },
  { "pointer-mode", gbp_glade_page_action_pointer_mode, "s" },
};

void
_gbp_glade_page_update_actions (GbpGladePage *self)
{
  GladeCommand *redo;
  GladeCommand *undo;

  g_assert (GBP_IS_GLADE_PAGE (self));
  g_assert (GLADE_IS_PROJECT (self->project));

  redo = glade_project_next_redo_item (self->project);
  undo = glade_project_next_undo_item (self->project);

  dzl_gtk_widget_action_set (GTK_WIDGET (self), "glade-view", "undo",
                             "enabled", undo != NULL,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self), "glade-view", "redo",
                             "enabled", redo != NULL,
                             NULL);
}

void
_gbp_glade_page_init_actions (GbpGladePage *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  g_assert (GBP_IS_GLADE_PAGE (self));

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "glade-view",
                                  G_ACTION_GROUP (group));

  _gbp_glade_page_update_actions (self);
}
