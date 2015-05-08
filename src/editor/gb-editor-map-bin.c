/* gb-editor-map-bin.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>
#include <ide.h>

#include "gb-editor-map-bin.h"

struct _GbEditorMapBin
{
  GtkBox     parent_instance;
  gint       cached_height;
  gulong     size_allocate_handler;
  GtkWidget *floating_bar;
  GtkWidget *separator;
};

G_DEFINE_TYPE (GbEditorMapBin, gb_editor_map_bin, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_FLOATING_BAR,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_editor_map_bin__floating_bar_size_allocate (GbEditorMapBin *self,
                                               GtkAllocation  *alloc,
                                               GtkWidget      *floating_bar)
{
  g_assert (GB_IS_EDITOR_MAP_BIN (self));
  g_assert (alloc != NULL);
  g_assert (GTK_IS_WIDGET (floating_bar));

  if (self->cached_height != alloc->height)
    {
      self->cached_height = alloc->height;
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

static void
gb_editor_map_bin_set_floating_bar (GbEditorMapBin *self,
                                    GtkWidget      *floating_bar)
{
  g_return_if_fail (GB_IS_EDITOR_MAP_BIN (self));

  if (floating_bar != self->floating_bar)
    {
      if (self->floating_bar)
        {
          ide_clear_signal_handler (self->floating_bar, &self->size_allocate_handler);
          ide_clear_weak_pointer (&self->floating_bar);
        }

      if (floating_bar)
        {
          ide_set_weak_pointer (&self->floating_bar, floating_bar);
          g_signal_connect_object (self->floating_bar,
                                   "size-allocate",
                                   G_CALLBACK (gb_editor_map_bin__floating_bar_size_allocate),
                                   self,
                                   G_CONNECT_SWAPPED);
        }

      gtk_widget_queue_resize (GTK_WIDGET (self));
    }

  if (ide_set_weak_pointer (&self->floating_bar, floating_bar))
    gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
gb_editor_map_bin_size_allocate (GtkWidget     *widget,
                                 GtkAllocation *alloc)
{
  GbEditorMapBin *self = (GbEditorMapBin *)widget;

  alloc->height -= self->cached_height;

  GTK_WIDGET_CLASS (gb_editor_map_bin_parent_class)->size_allocate (widget, alloc);
}

static void
gb_editor_map_bin_add (GtkContainer *container,
                       GtkWidget    *child)
{
  GbEditorMapBin *self = (GbEditorMapBin *)container;

  if (IDE_IS_SOURCE_MAP (child) && (self->separator != NULL))
    gtk_widget_show (GTK_WIDGET (self->separator));

  GTK_CONTAINER_CLASS (gb_editor_map_bin_parent_class)->add (container, child);
}

static void
gb_editor_map_bin_remove (GtkContainer *container,
                          GtkWidget    *child)
{
  GbEditorMapBin *self = (GbEditorMapBin *)container;

  if (IDE_IS_SOURCE_MAP (child) && (self->separator != NULL))
    gtk_widget_hide (GTK_WIDGET (self->separator));

  GTK_CONTAINER_CLASS (gb_editor_map_bin_parent_class)->remove (container, child);
}

static void
gb_editor_map_bin_finalize (GObject *object)
{
  GbEditorMapBin *self = (GbEditorMapBin *)object;

  if (self->separator != NULL)
    g_object_remove_weak_pointer (G_OBJECT (self->separator), (gpointer *)&self->separator);
  ide_clear_signal_handler (self->floating_bar, &self->size_allocate_handler);
  ide_clear_weak_pointer (&self->floating_bar);

  G_OBJECT_CLASS (gb_editor_map_bin_parent_class)->finalize (object);
}

static void
gb_editor_map_bin_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbEditorMapBin *self = GB_EDITOR_MAP_BIN (object);

  switch (prop_id)
    {
    case PROP_FLOATING_BAR:
      gb_editor_map_bin_set_floating_bar (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_map_bin_class_init (GbEditorMapBinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = gb_editor_map_bin_finalize;
  object_class->set_property = gb_editor_map_bin_set_property;

  widget_class->size_allocate = gb_editor_map_bin_size_allocate;

  container_class->add = gb_editor_map_bin_add;
  container_class->remove = gb_editor_map_bin_remove;

  gParamSpecs [PROP_FLOATING_BAR] =
    g_param_spec_object ("floating-bar",
                         "Floating Bar",
                         "The floating bar to use for relative allocation size.",
                         GTK_TYPE_WIDGET,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);
}

static void
gb_editor_map_bin_init (GbEditorMapBin *self)
{
  self->separator = g_object_new (GTK_TYPE_SEPARATOR,
                                  "orientation", GTK_ORIENTATION_VERTICAL,
                                  "hexpand", FALSE,
                                  "visible", FALSE,
                                  NULL);
  g_object_add_weak_pointer (G_OBJECT (self->separator), (gpointer *)&self->separator);
  gtk_container_add (GTK_CONTAINER (self), self->separator);
}
