/* gb-color-picker-prefs-palette-list.c
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

#include "gb-color-picker-prefs-palette-row.h"

#include "gb-color-picker-prefs-palette-list.h"

struct _GbColorPickerPrefsPaletteList
{
  GtkBox         parent_instance;

  GtkListBox    *list_box;
  GtkListBoxRow *plus_row;
  GtkWidget     *plus_button;
};

G_DEFINE_TYPE (GbColorPickerPrefsPaletteList, gb_color_picker_prefs_palette_list, GTK_TYPE_BOX)

enum {
  ADDED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

GbColorPickerPrefsPaletteList *
gb_color_picker_prefs_palette_list_new (void)
{
  return g_object_new (GB_TYPE_COLOR_PICKER_PREFS_PALETTE_LIST, NULL);
}

GtkListBox *
gb_color_picker_prefs_palette_list_get_list_box (GbColorPickerPrefsPaletteList *self)
{
  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_LIST (self));

  return self->list_box;
}

static void
gb_color_picker_prefs_palette_list_row_plus_button_pressed_cb (GbColorPickerPrefsPaletteList *self,
                                                               GtkButton                     *button)
{
  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_LIST (self));
  g_assert (GTK_IS_BUTTON (button));

  g_signal_emit (self, signals [ADDED], 0);
}

static void
gb_color_picker_prefs_palette_list_row_activated_cb (GbColorPickerPrefsPaletteList *self,
                                                     GtkListBoxRow                 *row,
                                                     GtkListBox                    *listbox)
{
  GtkWidget *child;

  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_LIST (self));
  g_assert (GTK_IS_LIST_BOX (listbox));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  child = gtk_bin_get_child (GTK_BIN (row));
  if (child != NULL)
    gtk_widget_activate (child);
}

static gboolean
gb_picker_prefs_palette_list_key_pressed_cb (GbColorPickerPrefsPaletteList *self,
                                             GdkEventKey                   *event,
                                             GtkListBox                    *list_box)
{
  GtkWidget *toplevel;
  GtkWidget *focused_widget;
  GtkWidget *row_child;
  gboolean is_editing;

  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_LIST (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_LIST_BOX (list_box));

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (list_box));
  if (!gtk_widget_is_toplevel (toplevel) || event->type != GDK_KEY_PRESS)
    return GDK_EVENT_PROPAGATE;

  focused_widget = gtk_window_get_focus (GTK_WINDOW (toplevel));
  if (gtk_widget_get_parent (focused_widget) == GTK_WIDGET (list_box))
    {
      row_child = gtk_bin_get_child (GTK_BIN (focused_widget));
      if (GB_IS_COLOR_PICKER_PREFS_PALETTE_ROW (row_child))
        {
          g_object_get (G_OBJECT (row_child), "is-editing", &is_editing, NULL);
          if (!is_editing && event->keyval == GDK_KEY_F2)
            {
              g_signal_emit_by_name (row_child, "edit");
              return GDK_EVENT_STOP;
            }
        }
    }

  return GDK_EVENT_PROPAGATE;
}

static void
gb_color_picker_prefs_palette_list_add (GtkContainer *container,
                                        GtkWidget    *widget)
{
  GbColorPickerPrefsPaletteList *self = (GbColorPickerPrefsPaletteList *)container;

  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_LIST (self));
  g_assert (GTK_IS_WIDGET (widget));

  gtk_list_box_insert (self->list_box, widget, -1);
}

static void
gb_color_picker_prefs_palette_list_init_ui (GbColorPickerPrefsPaletteList *self)
{
  GtkStyleContext *context;
  GtkWidget *scrolled_window;
  GtkWidget *image;

  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_LIST (self));

  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "list-add-symbolic",
                        "icon-size", GTK_ICON_SIZE_MENU,
                        "visible", TRUE,
                        NULL);
  self->plus_button = g_object_new (GTK_TYPE_BUTTON,
                                    "hexpand", TRUE,
                                    "visible", TRUE,
                                    NULL);

  gtk_container_add (GTK_CONTAINER (self->plus_button), image);

  context = gtk_widget_get_style_context (self->plus_button);
  gtk_style_context_add_class (context, "flat");

  scrolled_window = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                  "hscrollbar-policy", GTK_POLICY_NEVER,
                                  "propagate-natural-height", TRUE,
                                  "visible", TRUE,
                                  NULL);

  self->list_box = g_object_new (GTK_TYPE_LIST_BOX,
                                 "selection-mode", GTK_SELECTION_NONE,
                                 "visible", TRUE,
                                 NULL);

  gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (self->list_box));

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  /* As we overwrite the add vfunc we need to call its parent */
  GTK_CONTAINER_CLASS (gb_color_picker_prefs_palette_list_parent_class)->add (GTK_CONTAINER (self), self->plus_button);
  GTK_CONTAINER_CLASS (gb_color_picker_prefs_palette_list_parent_class)->add (GTK_CONTAINER (self), scrolled_window);
}

static void
gb_color_picker_prefs_palette_list_class_init (GbColorPickerPrefsPaletteListClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  signals [ADDED] =
    g_signal_new ("added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  container_class->add = gb_color_picker_prefs_palette_list_add;

  gtk_widget_class_set_css_name (widget_class, "gbcolorpickerprefspalettelist");
}

static void
gb_color_picker_prefs_palette_list_init (GbColorPickerPrefsPaletteList *self)
{
  gb_color_picker_prefs_palette_list_init_ui (self);

  g_signal_connect_swapped (self->list_box, "row-activated",
                            G_CALLBACK (gb_color_picker_prefs_palette_list_row_activated_cb),
                            self);

  g_signal_connect_swapped (self->list_box, "key-press-event",
                            G_CALLBACK (gb_picker_prefs_palette_list_key_pressed_cb),
                            self);

  g_signal_connect_swapped (self->plus_button, "pressed",
                            G_CALLBACK (gb_color_picker_prefs_palette_list_row_plus_button_pressed_cb),
                            self);
}
