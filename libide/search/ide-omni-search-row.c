/* ide-omni-search-row.c
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

#include "ide-omni-search-row.h"

struct _IdeOmniSearchRow
{
  GtkBox           parent_instance;

  IdeSearchResult *result;

  /* References owned by template */
  GtkLabel        *title;
  GtkImage        *image;
};

G_DEFINE_TYPE (IdeOmniSearchRow, ide_omni_search_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_ICON_NAME,
  PROP_RESULT,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_omni_search_row_set_icon_name (IdeOmniSearchRow *self,
                                   const gchar      *icon_name)
{
  g_assert (IDE_IS_OMNI_SEARCH_ROW (self));

  gtk_image_set_from_icon_name (self->image, icon_name, GTK_ICON_SIZE_MENU);
}

static void
ide_omni_search_row_connect (IdeOmniSearchRow *row,
                             IdeSearchResult  *result)
{
  const gchar *title;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_ROW (row));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

  title = ide_search_result_get_title (result);
  gtk_label_set_markup (row->title, title);
}

IdeSearchResult *
ide_omni_search_row_get_result (IdeOmniSearchRow *row)
{
  g_return_val_if_fail (IDE_IS_OMNI_SEARCH_ROW (row), NULL);

  return row->result;
}

void
ide_omni_search_row_set_result (IdeOmniSearchRow *row,
                                IdeSearchResult  *result)
{
  g_return_if_fail (IDE_IS_OMNI_SEARCH_ROW (row));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

  if (result != row->result)
    {
      g_clear_object (&row->result);

      if (result)
        {
          row->result = g_object_ref (result);
          ide_omni_search_row_connect (row, result);
        }

      g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_RESULT]);
    }
}

static void
ide_omni_search_row_finalize (GObject *object)
{
  IdeOmniSearchRow *self = (IdeOmniSearchRow *)object;

  g_clear_object (&self->result);

  G_OBJECT_CLASS (ide_omni_search_row_parent_class)->finalize (object);
}

static void
ide_omni_search_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeOmniSearchRow *self = IDE_OMNI_SEARCH_ROW (object);

  switch (prop_id)
    {
    case PROP_RESULT:
      g_value_set_object (value, ide_omni_search_row_get_result (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_omni_search_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeOmniSearchRow *self = IDE_OMNI_SEARCH_ROW (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      ide_omni_search_row_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_RESULT:
      ide_omni_search_row_set_result (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_omni_search_row_class_init (IdeOmniSearchRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_omni_search_row_finalize;
  object_class->get_property = ide_omni_search_row_get_property;
  object_class->set_property = ide_omni_search_row_set_property;

  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "Icon Name",
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RESULT] =
    g_param_spec_object ("result",
                         "Result",
                         "Result",
                         IDE_TYPE_SEARCH_RESULT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_css_name (widget_class, "omnisearchrow");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-omni-search-row.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeOmniSearchRow, image);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniSearchRow, title);
}

static void
ide_omni_search_row_init (IdeOmniSearchRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
