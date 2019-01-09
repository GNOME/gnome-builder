/* gstyle-color-panel-actions.c
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

#define G_LOG_DOMAIN "gstyle-color-panel"

#include "gstyle-color-panel-private.h"
#include "gstyle-color-panel.h"
#include "gstyle-slidein.h"

#include "gstyle-color-panel-actions.h"

static void
gstyle_color_panel_actions_toggle_page (GSimpleAction *action,
                                        GVariant      *variant,
                                        gpointer       user_data)
{
  GstyleColorPanel *self = (GstyleColorPanel *)user_data;
  g_autoptr (GVariant) value = NULL;
  g_autofree gchar *page_name = NULL;
  const gchar *name;
  gboolean state;

  g_assert (GSTYLE_IS_COLOR_PANEL (self));

  value = g_action_get_state (G_ACTION (action));
  state = g_variant_get_boolean (value);
  name = g_action_get_name(G_ACTION (action));
  if (!g_str_has_prefix (name, "toggle-"))
    return;

  page_name = g_strdup (&name [7]);
  g_simple_action_set_state (action, g_variant_new_boolean (!state));
  if (!state)
    {
      _gstyle_color_panel_update_prefs_page (self, page_name);
      gtk_stack_set_visible_child_name (self->prefs_stack, page_name);
    }

  gstyle_slidein_reveal_slide (GSTYLE_SLIDEIN (self->prefs_slidein),
                               !gstyle_slidein_get_revealed (GSTYLE_SLIDEIN (self->prefs_slidein)));
}

static GActionEntry ColorPanelPagesPrefsActions[] = {
  { "toggle-components-page", NULL, "b", "false", gstyle_color_panel_actions_toggle_page },
  { "toggle-colorstrings-page", NULL, "b", "false", gstyle_color_panel_actions_toggle_page },
  { "toggle-palettes-page", NULL, "b", "false", gstyle_color_panel_actions_toggle_page },
  { "toggle-paletteslist-page", NULL, "b", "false", gstyle_color_panel_actions_toggle_page },
};

void
gstyle_color_panel_actions_init (GstyleColorPanel *self)
{
  g_autoptr (GSimpleActionGroup) pages_group = NULL;
  GActionGroup *palette_widget_actions_group;

  pages_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (pages_group), ColorPanelPagesPrefsActions,
                                   G_N_ELEMENTS (ColorPanelPagesPrefsActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "gstyle-pages-prefs", G_ACTION_GROUP (pages_group));

  if (self->palette_widget != NULL)
    {
      palette_widget_actions_group = gtk_widget_get_action_group (GTK_WIDGET (self->palette_widget), "gstyle-palettes-prefs");
      if (palette_widget_actions_group != NULL)
        gtk_widget_insert_action_group (GTK_WIDGET (self),
                                        "gstyle-palettes-prefs",
                                        palette_widget_actions_group);
    }
}
