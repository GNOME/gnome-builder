/* gstyle-color-widget-actions.c
 *
 * Copyright 2016 Sebastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "gstyle-color-widget"

#include <glib/gi18n.h>
#include "gstyle-rename-popover.h"
#include "gstyle-palette-widget.h"

#include "gstyle-color-widget-actions.h"

static void
contextual_popover_closed_cb (GstyleColorWidget *self,
                              GtkWidget         *popover)
{
  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (GTK_IS_WIDGET (popover));

  gtk_widget_destroy (popover);
}

static void
rename_popover_entry_renamed_cb (GstyleColorWidget *self,
                                 const gchar       *name)
{
  GstyleColor *color;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));

  color = gstyle_color_widget_get_color (self);
  gstyle_color_set_name (color, name);
}

static void
gstyle_color_widget_actions_rename (GSimpleAction *action,
                                    GVariant      *variant,
                                    gpointer       user_data)
{
  GstyleColorWidget *self = (GstyleColorWidget *)user_data;
  GtkWidget *popover;
  GstyleColor *color;
  const gchar *name;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (G_IS_SIMPLE_ACTION (action));

  color = gstyle_color_widget_get_color (self);
  name = gstyle_color_get_name (color);

  popover = g_object_new (GSTYLE_TYPE_RENAME_POPOVER,
                          "label", _("Color name"),
                          "name", name,
                          "message", _("Enter a new name for the color"),
                          NULL);

  gtk_popover_set_relative_to (GTK_POPOVER (popover), GTK_WIDGET (self));
  g_signal_connect_swapped (popover, "closed", G_CALLBACK (contextual_popover_closed_cb), self);
  g_signal_connect_swapped (popover, "renamed", G_CALLBACK (rename_popover_entry_renamed_cb), self);
  gtk_popover_popup (GTK_POPOVER (popover));
}

static void
gstyle_color_widget_actions_remove (GSimpleAction *action,
                                    GVariant      *variant,
                                    gpointer       user_data)
{
  GstyleColorWidget *self = (GstyleColorWidget *)user_data;
  GtkWidget *ancestor;
  GstylePalette *selected_palette;
  GstyleColor *color;

  g_assert (GSTYLE_IS_COLOR_WIDGET (self));
  g_assert (G_IS_SIMPLE_ACTION (action));

  ancestor = gtk_widget_get_ancestor (GTK_WIDGET (self), GSTYLE_TYPE_PALETTE_WIDGET);
  if (ancestor != NULL)
    {
      color = gstyle_color_widget_get_color (self);
      selected_palette = gstyle_palette_widget_get_selected_palette (GSTYLE_PALETTE_WIDGET (ancestor));
      if (selected_palette != NULL && color != NULL)
        gstyle_palette_remove (selected_palette, color);
    }
}

static GActionEntry actions[] = {
  { "rename", gstyle_color_widget_actions_rename },
  { "remove", gstyle_color_widget_actions_remove }
};

void
gstyle_color_widget_actions_init (GstyleColorWidget *self)
{
  g_autoptr (GSimpleActionGroup) action_group = NULL;

  action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (action_group), actions,
                                   G_N_ELEMENTS (actions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "gstyle-color-widget-menu", G_ACTION_GROUP (action_group));
}
