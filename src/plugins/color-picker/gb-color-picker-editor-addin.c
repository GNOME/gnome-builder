/* gb-color-picker-editor-addin.c
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

#define G_LOG_DOMAIN "gb-color-picker-editor-addin"

#include <glib/gi18n.h>
#include <gstyle-color-panel.h>

#include "gb-color-picker-editor-addin.h"
#include "gb-color-picker-editor-page-addin.h"
#include "gb-color-picker-prefs.h"

struct _GbColorPickerEditorAddin
{
  GObject parent_instance;

  /*
   * An unowned reference to the editor. This is set/unset when
   * load/unload vfuncs are called.
   */
  IdeEditorSurface *editor;

  /*
   * Out preferences to use in conjunction with the pane. This needs
   * to be attached to the panel for the proper preferences to be
   * shown in the sidebar widgetry.
   */
  GbColorPickerPrefs *prefs;

  /*
   * Our transient panel which we will slide into visibility when
   * the current view is an IdeEditorPage with the color-picker
   * enabled.
   */
  GstyleColorPanel *panel;

  /*
   * This widget contains @panel but conforms to the DzlDockItem
   * interface necessary for adding items to the panel.
   */
  DzlDockWidget *dock;

  /*
   * If the current view in the surface is an editor view, then
   * this unowned reference will point to that view.
   */
  IdeEditorPage *view;

  /*
   * This signal group manages correctly binding/unbinding signals from
   * our peer addin attached to the view. It emits a ::color-found signal
   * which we use to propagate the color to the panel.
   */
  DzlSignalGroup *view_addin_signals;
};

/*
 * gb_color_picker_editor_addin_add_palette:
 * @self: a #GbColorPickerEditorAddin
 * @palette_widget: the widget to add the palette to
 * @uri: the uri of the file containing the palette
 *
 * Loads a palette found at @uri and adds it to @palette_widget.
 *
 * The palette instance returned is a full reference and should
 * be freed with g_object_unref().
 *
 * Returns: (transfer full): The newly loaded palette
 */
static GstylePalette *
gb_color_picker_editor_addin_add_palette (GbColorPickerEditorAddin *self,
                                          GstylePaletteWidget      *palette_widget,
                                          const gchar              *uri)
{
  g_autoptr(GstylePalette) palette = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;

  g_assert (GB_IS_COLOR_PICKER_EDITOR_ADDIN (self));
  g_assert (GSTYLE_PALETTE_WIDGET (palette_widget));
  g_assert (!dzl_str_empty0 (uri));

  file = g_file_new_for_uri (uri);
  palette = gstyle_palette_new_from_file (file, NULL, &error);

  if (palette == NULL)
    {
      g_warning ("Unable to load palette: %s", error->message);
      return NULL;
    }

  gstyle_palette_widget_add (palette_widget, palette);

  return g_steal_pointer (&palette);
}

static const gchar * internal_palettes[] = {
  "resource:///plugins/color-picker/data/basic.gstyle.xml",
  "resource:///plugins/color-picker/data/svg.gpl",
};

static void
gb_color_picker_editor_addin_init_palettes (GbColorPickerEditorAddin *self)
{
  GstylePaletteWidget *palette_widget;

  g_assert (GB_IS_COLOR_PICKER_EDITOR_ADDIN (self));

  palette_widget = gstyle_color_panel_get_palette_widget (self->panel);

  for (guint i = 0; i < G_N_ELEMENTS (internal_palettes); i++)
    {
      const gchar *uri = internal_palettes[i];
      g_autoptr(GstylePalette) palette = NULL;

      palette = gb_color_picker_editor_addin_add_palette (self, palette_widget, uri);

      /* Show the last panel in the list */
      if (i + 1 == G_N_ELEMENTS (internal_palettes))
        gstyle_color_panel_show_palette (self->panel, palette);
    }
}

static void
gb_color_picker_editor_addin_notify_rgba (GbColorPickerEditorAddin *self,
                                          GParamSpec               *pspec,
                                          GstyleColorPanel         *panel)
{
  g_autoptr(GstyleColor) color = NULL;
  GdkRGBA rgba;

  g_assert (GB_IS_COLOR_PICKER_EDITOR_ADDIN (self));
  g_assert (pspec != NULL);
  g_assert (GSTYLE_IS_COLOR_PANEL (panel));

  gstyle_color_panel_get_rgba (self->panel, &rgba);
  color = gstyle_color_new_from_rgba (NULL, GSTYLE_COLOR_KIND_RGB_HEX6, &rgba);

  if (self->view_addin_signals != NULL)
    {
      GbColorPickerEditorPageAddin *view_addin;

      view_addin = dzl_signal_group_get_target (self->view_addin_signals);

      if (GB_IS_COLOR_PICKER_EDITOR_PAGE_ADDIN (view_addin))
        gb_color_picker_editor_page_addin_set_color (view_addin, color);
    }
}

static void
gb_color_picker_editor_addin_set_panel (GbColorPickerEditorAddin *self)
{
  g_assert (GB_IS_COLOR_PICKER_EDITOR_ADDIN (self));

  self->panel = g_object_new (GSTYLE_TYPE_COLOR_PANEL,
                              "visible", TRUE,
                              NULL);
  g_signal_connect (self->panel,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->panel);
  g_signal_connect_object (self->panel,
                           "notify::rgba",
                           G_CALLBACK (gb_color_picker_editor_addin_notify_rgba),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_container_add (GTK_CONTAINER (self->dock), GTK_WIDGET (self->panel));

  self->prefs = g_object_new (GB_TYPE_COLOR_PICKER_PREFS,
                              "panel", self->panel,
                              NULL);

  gb_color_picker_editor_addin_init_palettes (self);
}

static void
gb_color_picker_editor_addin_show_panel (GbColorPickerEditorAddin *self)
{
  g_assert (GB_IS_COLOR_PICKER_EDITOR_ADDIN (self));

  if (self->view != NULL)
    {
      IdeTransientSidebar *sidebar;
      IdePage *view = IDE_PAGE (self->view);

      if (self->panel == NULL)
        gb_color_picker_editor_addin_set_panel (self);

      sidebar = ide_editor_surface_get_transient_sidebar (self->editor);

      ide_transient_sidebar_set_page (sidebar, view);
      ide_transient_sidebar_set_panel (sidebar, GTK_WIDGET (self->dock));

      g_object_set (self->editor, "right-visible", TRUE, NULL);
    }
}

static void
gb_color_picker_editor_addin_hide_panel (GbColorPickerEditorAddin *self)
{
  g_assert (GB_IS_COLOR_PICKER_EDITOR_ADDIN (self));

  /* For the case we'll add more code here */
  if (self->panel == NULL)
    return;

  g_object_set (self->editor, "right-visible", FALSE, NULL);
}

static void
gb_color_picker_editor_addin_notify_enabled (GbColorPickerEditorAddin     *self,
                                             GParamSpec                   *pspec,
                                             GbColorPickerEditorPageAddin *view_addin)
{
  g_assert (GB_IS_COLOR_PICKER_EDITOR_ADDIN (self));
  g_assert (GB_IS_COLOR_PICKER_EDITOR_PAGE_ADDIN (view_addin));

  /* This function is called when the enabled state is toggled
   * for the specific view in question. We hide the panel if it
   * is current visible, otherwise we show it.
   */

  if (gb_color_picker_editor_page_addin_get_enabled (view_addin))
    gb_color_picker_editor_addin_show_panel (self);
  else
    gb_color_picker_editor_addin_hide_panel (self);
}

static void
gb_color_picker_editor_addin_color_found (GbColorPickerEditorAddin     *self,
                                          GstyleColor                  *color,
                                          GbColorPickerEditorPageAddin *view_addin)
{
  GdkRGBA rgba;

  g_assert (GB_IS_COLOR_PICKER_EDITOR_ADDIN (self));
  g_assert (GSTYLE_IS_COLOR (color));
  g_assert (GB_IS_COLOR_PICKER_EDITOR_PAGE_ADDIN (view_addin));

  dzl_signal_group_block (self->view_addin_signals);
  gstyle_color_fill_rgba (color, &rgba);

  if (self->panel == NULL)
    gb_color_picker_editor_addin_set_panel (self);

  gstyle_color_panel_set_rgba (self->panel, &rgba);
  dzl_signal_group_unblock (self->view_addin_signals);
}

static void
gb_color_picker_editor_addin_load (IdeEditorAddin       *addin,
                                   IdeEditorSurface *surface)
{
  GbColorPickerEditorAddin *self = (GbColorPickerEditorAddin *)addin;
  IdeTransientSidebar *sidebar;

  g_assert (GB_IS_COLOR_PICKER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_SURFACE (surface));

  self->editor = surface;
  self->view_addin_signals = dzl_signal_group_new (GB_TYPE_COLOR_PICKER_EDITOR_PAGE_ADDIN);
  dzl_signal_group_connect_swapped (self->view_addin_signals,
                                    "color-found",
                                    G_CALLBACK (gb_color_picker_editor_addin_color_found),
                                    self);

  dzl_signal_group_connect_swapped (self->view_addin_signals,
                                    "notify::enabled",
                                    G_CALLBACK (gb_color_picker_editor_addin_notify_enabled),
                                    self);

  self->dock = g_object_new (DZL_TYPE_DOCK_WIDGET,
                             "title", _("Colors"),
                             "icon-name", "preferences-color-symbolic",
                             "vexpand", TRUE,
                             "visible", TRUE,
                             NULL);
  g_signal_connect (self->dock,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->dock);

  sidebar = ide_editor_surface_get_transient_sidebar (self->editor);
  gtk_container_add (GTK_CONTAINER (sidebar), GTK_WIDGET (self->dock));
}


static void
gb_color_picker_editor_addin_unload (IdeEditorAddin       *addin,
                                     IdeEditorSurface *surface)
{
  GbColorPickerEditorAddin *self = (GbColorPickerEditorAddin *)addin;

  g_assert (GB_IS_COLOR_PICKER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_SURFACE (surface));

  g_clear_object (&self->view_addin_signals);

  if (self->dock != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->dock));

  if (self->panel != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->panel));

  g_clear_object (&self->prefs);

  self->editor = NULL;
}

static void
gb_color_picker_editor_addin_page_set (IdeEditorAddin *addin,
                                       IdePage  *view)
{
  GbColorPickerEditorAddin *self = (GbColorPickerEditorAddin *)addin;

  g_assert (GB_IS_COLOR_PICKER_EDITOR_ADDIN (self));
  g_assert (!view || IDE_IS_PAGE (view));

  if (IDE_IS_EDITOR_PAGE (view))
    {
      IdeEditorPageAddin *view_addin;

      self->view = IDE_EDITOR_PAGE (view);

      /* The addin may not be available yet if things are just initializing.
       * We'll have to wait for a follow up view-set to make progress.
       */
      view_addin = ide_editor_page_addin_find_by_module_name (self->view, "color-picker");
      g_assert (!view_addin || GB_IS_COLOR_PICKER_EDITOR_PAGE_ADDIN (view_addin));

      dzl_signal_group_set_target (self->view_addin_signals, view_addin);

      if (view_addin != NULL &&
          gb_color_picker_editor_page_addin_get_enabled (GB_COLOR_PICKER_EDITOR_PAGE_ADDIN (view_addin)))
        gb_color_picker_editor_addin_show_panel (self);
    }
  else
    {
      self->view = NULL;
      dzl_signal_group_set_target (self->view_addin_signals, NULL);
      gb_color_picker_editor_addin_hide_panel (self);
    }
}

static void
editor_addin_iface_init (IdeEditorAddinInterface *iface)
{
  iface->load = gb_color_picker_editor_addin_load;
  iface->unload = gb_color_picker_editor_addin_unload;
  iface->page_set = gb_color_picker_editor_addin_page_set;
}

G_DEFINE_TYPE_WITH_CODE (GbColorPickerEditorAddin,
                         gb_color_picker_editor_addin,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_ADDIN,
                                                editor_addin_iface_init))

static void
gb_color_picker_editor_addin_class_init (GbColorPickerEditorAddinClass *klass)
{
}

static void
gb_color_picker_editor_addin_init (GbColorPickerEditorAddin *self)
{
}

/**
 * gb_color_picker_editor_addin_create_palette:
 * @self: a #GbColorPickerEditorAddin
 *
 * Creates a new #GstylePalette for the currently focused editor view.
 *
 * If no editor view is focused, %NULL is returned.
 *
 * Returns: (transfer full): a #GstylePalette or %NULL.
 *
 * Since: 3.26
 */
GstylePalette *
gb_color_picker_editor_addin_create_palette (GbColorPickerEditorAddin *self)
{
  g_return_val_if_fail (GB_IS_COLOR_PICKER_EDITOR_ADDIN (self), NULL);

  if (self->view != NULL)
    {
      IdeBuffer *buffer = ide_editor_page_get_buffer (self->view);

      return gstyle_palette_new_from_buffer (GTK_TEXT_BUFFER (buffer),
                                             NULL, NULL, NULL, NULL);
    }

  return NULL;
}
