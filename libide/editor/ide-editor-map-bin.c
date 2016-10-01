/* ide-editor-map-bin.c
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

#define G_LOG_DOMAIN "ide-editor-map-bin"

#include "config.h"

#include <glib/gi18n.h>
#include <pango/pangofc-fontmap.h>

#include "ide-macros.h"

#include "editor/ide-editor-map-bin.h"
#include "sourceview/ide-source-map.h"

struct _IdeEditorMapBin
{
  GtkBox     parent_instance;
  gint       cached_height;
  gulong     size_allocate_handler;
  GtkWidget *floating_bar;
  GtkWidget *separator;
};

G_DEFINE_TYPE (IdeEditorMapBin, ide_editor_map_bin, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_FLOATING_BAR,
  LAST_PROP
};

static FcConfig *localFontConfig;
static GParamSpec *properties [LAST_PROP];

static void
ide_editor_map_bin__floating_bar_size_allocate (IdeEditorMapBin *self,
                                               GtkAllocation  *alloc,
                                               GtkWidget      *floating_bar)
{
  g_assert (IDE_IS_EDITOR_MAP_BIN (self));
  g_assert (alloc != NULL);
  g_assert (GTK_IS_WIDGET (floating_bar));

  if (self->cached_height != alloc->height)
    {
      self->cached_height = alloc->height;
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

static void
ide_editor_map_bin_set_floating_bar (IdeEditorMapBin *self,
                                    GtkWidget      *floating_bar)
{
  g_return_if_fail (IDE_IS_EDITOR_MAP_BIN (self));

  if (floating_bar != self->floating_bar)
    {
      self->cached_height = 0;

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
                                   G_CALLBACK (ide_editor_map_bin__floating_bar_size_allocate),
                                   self,
                                   G_CONNECT_SWAPPED);
          gtk_widget_queue_resize (GTK_WIDGET (floating_bar));
        }

      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

static void
ide_editor_map_bin_size_allocate (GtkWidget     *widget,
                                 GtkAllocation *alloc)
{
  IdeEditorMapBin *self = (IdeEditorMapBin *)widget;

  if (self->floating_bar != NULL)
    alloc->height -= self->cached_height;

  GTK_WIDGET_CLASS (ide_editor_map_bin_parent_class)->size_allocate (widget, alloc);
}

static void
ide_editor_map_bin_add (GtkContainer *container,
                        GtkWidget    *child)
{
  IdeEditorMapBin *self = (IdeEditorMapBin *)container;

  if (IDE_IS_SOURCE_MAP (child) && (self->separator != NULL))
    {
      PangoFontMap *font_map;
      PangoFontDescription *font_desc;

      font_map = pango_cairo_font_map_new_for_font_type (CAIRO_FONT_TYPE_FT);
      pango_fc_font_map_set_config (PANGO_FC_FONT_MAP (font_map), localFontConfig);
      gtk_widget_set_font_map (child, font_map);

      font_desc = pango_font_description_from_string ("Builder Blocks 1");
      g_object_set (child, "font-desc", font_desc, NULL);

      g_object_unref (font_map);
      pango_font_description_free (font_desc);

      gtk_widget_show (GTK_WIDGET (self->separator));
    }

  GTK_CONTAINER_CLASS (ide_editor_map_bin_parent_class)->add (container, child);
}

static void
ide_editor_map_bin_remove (GtkContainer *container,
                           GtkWidget    *child)
{
  IdeEditorMapBin *self = (IdeEditorMapBin *)container;

  if (IDE_IS_SOURCE_MAP (child) && (self->separator != NULL))
    gtk_widget_hide (GTK_WIDGET (self->separator));

  GTK_CONTAINER_CLASS (ide_editor_map_bin_parent_class)->remove (container, child);
}

static void
ide_editor_map_bin_finalize (GObject *object)
{
  IdeEditorMapBin *self = (IdeEditorMapBin *)object;

  if (self->separator != NULL)
    g_object_remove_weak_pointer (G_OBJECT (self->separator), (gpointer *)&self->separator);
  ide_clear_signal_handler (self->floating_bar, &self->size_allocate_handler);
  ide_clear_weak_pointer (&self->floating_bar);

  G_OBJECT_CLASS (ide_editor_map_bin_parent_class)->finalize (object);
}

static void
ide_editor_map_bin_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  IdeEditorMapBin *self = IDE_EDITOR_MAP_BIN (object);

  switch (prop_id)
    {
    case PROP_FLOATING_BAR:
      g_value_set_object (value, self->floating_bar);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_map_bin_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  IdeEditorMapBin *self = IDE_EDITOR_MAP_BIN (object);

  switch (prop_id)
    {
    case PROP_FLOATING_BAR:
      ide_editor_map_bin_set_floating_bar (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_map_bin_load_font (void)
{
  const gchar *font_path = PACKAGE_DATADIR"/gnome-builder/fonts/BuilderBlocks.ttf";

  localFontConfig = FcInitLoadConfigAndFonts ();

  if (g_getenv ("GB_IN_TREE_FONTS") != NULL)
    font_path = "data/fonts/BuilderBlocks.ttf";

  FcConfigAppFontAddFile (localFontConfig, (const FcChar8 *)font_path);
}

static void
ide_editor_map_bin_class_init (IdeEditorMapBinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = ide_editor_map_bin_finalize;
  object_class->get_property = ide_editor_map_bin_get_property;
  object_class->set_property = ide_editor_map_bin_set_property;

  widget_class->size_allocate = ide_editor_map_bin_size_allocate;

  container_class->add = ide_editor_map_bin_add;
  container_class->remove = ide_editor_map_bin_remove;

  properties [PROP_FLOATING_BAR] =
    g_param_spec_object ("floating-bar",
                         "Floating Bar",
                         "The floating bar to use for relative allocation size.",
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  ide_editor_map_bin_load_font ();
}

static void
ide_editor_map_bin_init (IdeEditorMapBin *self)
{
  self->separator = g_object_new (GTK_TYPE_SEPARATOR,
                                  "orientation", GTK_ORIENTATION_VERTICAL,
                                  "hexpand", FALSE,
                                  "visible", FALSE,
                                  NULL);
  g_object_add_weak_pointer (G_OBJECT (self->separator), (gpointer *)&self->separator);
  gtk_container_add (GTK_CONTAINER (self), self->separator);
}
