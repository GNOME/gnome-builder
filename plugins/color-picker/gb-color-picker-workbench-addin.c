/* gb-color-picker-workbench_addin.c
 *
 * Copyright (C) 2016 sebastien lafargue <slafargue@gnome.org>
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
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <ide.h>

#include "gstyle-color.h"
#include "gstyle-color-panel.h"
#include "gstyle-palette.h"
#include "gstyle-palette-widget.h"

#include "gb-color-picker-document-monitor.h"
#include "gb-color-picker-prefs.h"

#include "gb-color-picker-workbench-addin.h"
#include "gb-color-picker-workbench-addin-private.h"

typedef struct __ViewState
{
  gboolean                      state;
  GbColorPickerDocumentMonitor *monitor;
} ViewState;

static void workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbColorPickerWorkbenchAddin, gb_color_picker_workbench_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static GstylePalette *
add_palette (GbColorPickerWorkbenchAddin *self,
             GstylePaletteWidget         *palette_widget,
             const gchar                 *uri)
{
  GstylePalette *palette;
  g_autoptr (GFile) file = NULL;
  GError *error = NULL;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));
  g_assert (GSTYLE_PALETTE_WIDGET (palette_widget));
  g_assert (!ide_str_empty0 (uri));

  file = g_file_new_for_uri (uri);
  palette = gstyle_palette_new_from_file (file, NULL, &error);
  if (palette == NULL)
    {
      g_assert (error != NULL);

      g_warning ("Unable to load the palette: %s\n", error->message);
      g_error_free (error);

      return NULL;
    }

  gstyle_palette_widget_add (palette_widget, palette);
  g_object_unref (palette);

  return palette;
}

static void
init_palettes (GbColorPickerWorkbenchAddin *self)
{
  GstylePaletteWidget *palette_widget;
  GstylePalette *palette;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));

  palette_widget = gstyle_color_panel_get_palette_widget (GSTYLE_COLOR_PANEL (self->color_panel));
  add_palette (self, palette_widget, "resource:///org/gnome/builder/plugins/color-picker-plugin/data/basic.gstyle.xml");
  palette = add_palette (self, palette_widget, "resource:///org/gnome/builder/plugins/color-picker-plugin/data/svg.gpl");

  gstyle_color_panel_show_palette (GSTYLE_COLOR_PANEL (self->color_panel), palette);
}

static void
set_menu_action_state (GbColorPickerWorkbenchAddin *self,
                       IdeEditorView               *view,
                       gboolean                     state)
{
  GActionGroup *group;
  GAction *menu_action;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  group = gtk_widget_get_action_group (GTK_WIDGET (view), "view");
  if (group != NULL)
    {
      menu_action = g_action_map_lookup_action(G_ACTION_MAP (group), "activate-color-picker");
     if (menu_action != NULL)
       g_action_change_state (menu_action, g_variant_new_boolean (state));
    }
}

static gboolean
get_menu_action_state (GbColorPickerWorkbenchAddin *self,
                       IdeEditorView               *view)
{
  GActionGroup *group;
  GAction *menu_action;
  gboolean state = FALSE;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  group = gtk_widget_get_action_group (GTK_WIDGET (view), "view");
  if (group != NULL)
    {
      menu_action = g_action_map_lookup_action(G_ACTION_MAP (group), "activate-color-picker");
     if (menu_action != NULL)
       state = g_variant_get_boolean (g_action_get_state (menu_action));
    }

  return state;
}

static GbColorPickerDocumentMonitor *
get_view_monitor (GbColorPickerWorkbenchAddin *self,
                  IdeEditorView               *view)
{
  GbColorPickerDocumentMonitor *monitor;
  IdeBuffer *buffer;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  buffer = ide_editor_view_get_buffer (view);
  if (buffer == NULL)
    return NULL;

  monitor = g_object_get_data (G_OBJECT (buffer), "monitor");
  return monitor;
}

static void
color_panel_rgba_set_cb (GbColorPickerWorkbenchAddin *self,
                         GParamSpec                  *prop)
{
  GbColorPickerDocumentMonitor *monitor;
  g_autoptr (GstyleColor) color = NULL;
  GdkRGBA rgba;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));

  gstyle_color_panel_get_rgba (GSTYLE_COLOR_PANEL (self->color_panel), &rgba);
  color = gstyle_color_new_from_rgba (NULL, GSTYLE_COLOR_KIND_RGB_HEX6, &rgba);

  monitor = get_view_monitor (self, IDE_EDITOR_VIEW (self->active_view));
  if (monitor != NULL)
    gb_color_picker_document_monitor_set_color_tag_at_cursor (monitor, color);
}

static gboolean
init_dock (GbColorPickerWorkbenchAddin *self)
{
  IdeLayoutTransientSidebar *sidebar;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));

  self->dock = g_object_new (DZL_TYPE_DOCK_WIDGET,
                             "title", _("Colors"),
                             "expand", TRUE,
                             "visible", TRUE,
                             NULL);
  self->color_panel = g_object_new (GSTYLE_TYPE_COLOR_PANEL,
                                    "visible", TRUE,
                                    NULL);

  self->prefs = g_object_new (GB_TYPE_COLOR_PICKER_PREFS,
                              "panel", self->color_panel,
                              "addin", self,
                              NULL);

  init_palettes (self);

  sidebar = ide_editor_perspective_get_transient_sidebar (IDE_EDITOR_PERSPECTIVE (self->editor));
  gtk_container_add (GTK_CONTAINER (sidebar), GTK_WIDGET (self->dock));
  gtk_container_add (GTK_CONTAINER (self->dock), self->color_panel);

  g_signal_connect_object (self->color_panel,
                           "notify::rgba",
                           G_CALLBACK (color_panel_rgba_set_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->dock_count = 1;
  return TRUE;
}

static gboolean
remove_dock (GbColorPickerWorkbenchAddin *self)
{
  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));

  self->dock_count = 0;
  self->color_panel = NULL;
  if (self->dock == NULL)
    return FALSE;

  gb_color_picker_prefs_set_panel (self->prefs, NULL);
  g_object_unref (self->prefs);
  gtk_widget_destroy (self->dock);
  self->dock = NULL;

  return TRUE;
}

static void
monitor_color_found_cb (GbColorPickerWorkbenchAddin  *self,
                        GstyleColor                  *color,
                        GbColorPickerDocumentMonitor *monitor)
{
  IdeBuffer *active_buffer;
  GdkRGBA rgba;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));
  g_assert (GB_IS_COLOR_PICKER_DOCUMENT_MONITOR (monitor));
  g_assert (GSTYLE_IS_COLOR (color));

  if (self->active_view == NULL)
    return;

  active_buffer = ide_editor_view_get_buffer (IDE_EDITOR_VIEW (self->active_view));
  if (active_buffer != NULL && self->dock != NULL)
    {
      gstyle_color_fill_rgba (color, &rgba);

      g_signal_handlers_block_by_func (self->color_panel, color_panel_rgba_set_cb, self);
      gstyle_color_panel_set_rgba (GSTYLE_COLOR_PANEL (self->color_panel), &rgba);
      g_signal_handlers_unblock_by_func (self->color_panel, color_panel_rgba_set_cb, self);
    }
}

static void
view_clear_cb (GtkWidget                   *widget,
               GbColorPickerWorkbenchAddin *self)
{
  IdeEditorView *view;
  GActionGroup *group;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (widget));

  view = IDE_EDITOR_VIEW (widget);

  group = gtk_widget_get_action_group (widget, "view");
  if (group != NULL)
    g_action_map_remove_action (G_ACTION_MAP (group), "activate-color-picker");

  g_hash_table_remove (self->views, view);
}

static void
view_clear (GbColorPickerWorkbenchAddin *self,
            IdeEditorView               *view,
            gboolean                     remove_color)
{
  GbColorPickerDocumentMonitor *monitor;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  monitor = get_view_monitor (self, view);
  if (monitor != NULL)
    {
      if (remove_color)
        gb_color_picker_document_monitor_uncolorize (monitor, NULL, NULL);

    self->monitor_count -= 1;
    if (self->monitor_count == 0)
      g_object_unref (monitor);
    }
}

static void
view_remove_dock (GbColorPickerWorkbenchAddin *self,
                  IdeEditorView               *view)
{
  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  if (self->dock == NULL)
    return;

  if (--self->dock_count <= 0)
    remove_dock (self);
  else
    {
      /* TODO: use insensitive panel state */
      gtk_widget_set_opacity (GTK_WIDGET (self->dock), 0.2);
    }
}

static void
activate_color_picker_action_cb (GbColorPickerWorkbenchAddin *self,
                                 GVariant                    *param,
                                 GSimpleAction               *menu_action)
{
  GbColorPickerDocumentMonitor *monitor;
  IdeEditorView *view;
  gboolean state;
  IdeBuffer *buffer;
  ViewState *view_state;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));
  g_assert (G_IS_SIMPLE_ACTION (menu_action));

  view = g_object_get_data (G_OBJECT (menu_action), "view");
  if (view == NULL || !IDE_IS_EDITOR_VIEW (view))
    return;

  state = get_menu_action_state (self, view);
  if (!state)
    {
      if (self->dock != NULL)
        {
          ++self->dock_count;
          gtk_widget_set_sensitive (GTK_WIDGET (self->dock), TRUE);
        }
      else
        init_dock (self);

      buffer = ide_editor_view_get_buffer (view);
      monitor = g_object_get_data (G_OBJECT (buffer), "monitor");
      if (monitor == NULL)
        {
          monitor = gb_color_picker_document_monitor_new (buffer);
          g_object_set_data (G_OBJECT (buffer), "monitor", monitor);
          g_signal_connect_object (monitor,
                                   "color-found",
                                   G_CALLBACK (monitor_color_found_cb), self,
                                   G_CONNECT_SWAPPED);
        }
      else
        g_object_ref (monitor);

      ide_workbench_focus (self->workbench, GTK_WIDGET (self->dock));
      gb_color_picker_document_monitor_colorize (monitor, NULL, NULL);
    }
  else
    {
      view_clear (self, view, TRUE);
      view_remove_dock (self, view);
    }

  view_state = g_hash_table_lookup (self->views, view);
  view_state->state = !state;
  set_menu_action_state (self, view, !state);

  if (self->dock != NULL)
    gtk_widget_set_opacity (GTK_WIDGET (self->dock), !state ? 1 : 0.2);
}

static void
setup_view_cb (GtkWidget                   *widget,
               GbColorPickerWorkbenchAddin *self)
{
  IdeEditorView *view = (IdeEditorView *)widget;
  GActionGroup *group;
  GSimpleAction *menu_action;
  ViewState *view_state;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_EDITOR_VIEW (view));

  view_state = g_new0 (ViewState, 1);
  view_state->state = FALSE;
  g_hash_table_insert (self->views, view, view_state);

  menu_action = g_simple_action_new_stateful ("activate-color-picker",
                                              NULL,
                                              g_variant_new_boolean(FALSE));

  group = gtk_widget_get_action_group (widget, "view");
  g_object_set_data (G_OBJECT (menu_action), "view", view);

  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (menu_action));
  set_menu_action_state (self, view, FALSE);
  g_signal_connect_object (menu_action,
                           "activate",
                           G_CALLBACK (activate_color_picker_action_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
view_added_cb (GbColorPickerWorkbenchAddin *self,
               GtkWidget                   *widget)
{
  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));

  if (!IDE_IS_EDITOR_VIEW (widget))
    return;

  setup_view_cb (widget, self);
}

static void
view_removed_cb (GbColorPickerWorkbenchAddin *self,
                 GtkWidget                   *widget)
{
  IdeLayoutView *view = (IdeLayoutView *)widget;
  ViewState *view_state;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_LAYOUT_VIEW (view));

  if (!IDE_IS_EDITOR_VIEW (view))
    return;

  view_state = g_hash_table_lookup (self->views, view);
  if (view != NULL && view_state->state)
    {
      view_clear (self, IDE_EDITOR_VIEW (view), FALSE);
      view_remove_dock (self, IDE_EDITOR_VIEW (view));
    }

  g_hash_table_remove (self->views, view);
}

static void
active_view_changed_cb (GbColorPickerWorkbenchAddin *self,
                        GParamSpec                  *prop)
{
  gboolean state;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (self));

  self->active_view = ide_editor_perspective_get_active_view (self->editor);
  if (self->active_view != NULL && IDE_IS_EDITOR_VIEW (self->active_view))
    {
      state = get_menu_action_state (self, IDE_EDITOR_VIEW (self->active_view));
      if (self->dock != NULL && self->dock_count > 0)
        {
          /* TODO: use insensitive panel state */
          gtk_widget_set_opacity (GTK_WIDGET (self->dock), state ? 1 : 0.2);
        }
    }
}

static void
gb_color_picker_workbench_addin_load (IdeWorkbenchAddin *addin,
                                      IdeWorkbench      *workbench)
{
  GbColorPickerWorkbenchAddin *self = (GbColorPickerWorkbenchAddin *)addin;
  IdeLayoutGrid *grid;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_WORKBENCH (workbench));

  ide_set_weak_pointer (&self->workbench, workbench);
  self->editor = IDE_EDITOR_PERSPECTIVE (ide_workbench_get_perspective_by_name (workbench, "editor"));
  grid = ide_editor_perspective_get_grid (self->editor);

  ide_perspective_views_foreach (IDE_PERSPECTIVE (self->editor), (GtkCallback)setup_view_cb, self);
  self->active_view = ide_layout_grid_get_current_view (grid);

  g_signal_connect_object (grid,
                           "view-added",
                           G_CALLBACK (view_added_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (grid,
                           "view-removed",
                           G_CALLBACK (view_removed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (grid,
                           "notify::current-view",
                           G_CALLBACK (active_view_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gb_color_picker_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                        IdeWorkbench       *workbench)
{
  GbColorPickerWorkbenchAddin *self = (GbColorPickerWorkbenchAddin *)addin;

  g_assert (GB_IS_COLOR_PICKER_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_WORKBENCH (workbench));

  ide_perspective_views_foreach (IDE_PERSPECTIVE (self->editor), (GtkCallback)view_clear_cb, self);

  remove_dock (self);
  g_hash_table_unref (self->views);

  ide_clear_weak_pointer (&self->workbench);
}

static void
gb_color_picker_workbench_addin_class_init (GbColorPickerWorkbenchAddinClass *klass)
{
}

static void
gb_color_picker_workbench_addin_init (GbColorPickerWorkbenchAddin *self)
{
  self->views = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gb_color_picker_workbench_addin_load;
  iface->unload = gb_color_picker_workbench_addin_unload;
}
