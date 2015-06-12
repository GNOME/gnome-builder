/* gb-search-display-row.c
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

#include "gb-search-display-row.h"
#include "gb-widget.h"

struct _GbSearchDisplayRow
{
  GtkBox           parent_instance;

  IdeSearchResult *result;

  /* References owned by template */
  GtkLabel        *title;
  GtkLabel        *subtitle;
  GtkProgressBar  *progress;
};

G_DEFINE_TYPE (GbSearchDisplayRow, gb_search_display_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_RESULT,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_search_display_row_connect (GbSearchDisplayRow *row,
                               IdeSearchResult    *result)
{
  const gchar *title;
  const gchar *subtitle;
  gfloat fraction;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY_ROW (row));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

  title = ide_search_result_get_title (result);
  gtk_label_set_markup (row->title, title);

  subtitle = ide_search_result_get_subtitle (result);
  if (subtitle)
    gtk_label_set_markup (row->subtitle, subtitle);
  gtk_widget_set_visible (GTK_WIDGET (row->subtitle), !!subtitle);

  fraction = ide_search_result_get_score (result);
  gtk_progress_bar_set_fraction (row->progress, fraction);
  gtk_widget_set_visible (GTK_WIDGET (row->progress), (fraction > 0.0));
}

IdeSearchResult *
gb_search_display_row_get_result (GbSearchDisplayRow *row)
{
  g_return_val_if_fail (GB_IS_SEARCH_DISPLAY_ROW (row), NULL);

  return row->result;
}

void
gb_search_display_row_set_result (GbSearchDisplayRow *row,
                                  IdeSearchResult    *result)
{
  g_return_if_fail (GB_IS_SEARCH_DISPLAY_ROW (row));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

  if (result != row->result)
    {
      g_clear_object (&row->result);

      if (result)
        {
          row->result = g_object_ref (result);
          gb_search_display_row_connect (row, result);
        }

      g_object_notify_by_pspec (G_OBJECT (row), gParamSpecs [PROP_RESULT]);
    }
}

static void
gb_search_display_row_finalize (GObject *object)
{
  GbSearchDisplayRow *self = (GbSearchDisplayRow *)object;

  g_clear_object (&self->result);

  G_OBJECT_CLASS (gb_search_display_row_parent_class)->finalize (object);
}

static void
gb_search_display_row_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GbSearchDisplayRow *self = GB_SEARCH_DISPLAY_ROW (object);

  switch (prop_id)
    {
    case PROP_RESULT:
      g_value_set_object (value, gb_search_display_row_get_result (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_display_row_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GbSearchDisplayRow *self = GB_SEARCH_DISPLAY_ROW (object);

  switch (prop_id)
    {
    case PROP_RESULT:
      gb_search_display_row_set_result (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_display_row_class_init (GbSearchDisplayRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_search_display_row_finalize;
  object_class->get_property = gb_search_display_row_get_property;
  object_class->set_property = gb_search_display_row_set_property;

  gParamSpecs [PROP_RESULT] =
    g_param_spec_object ("result",
                         _("Result"),
                         _("Result"),
                         IDE_TYPE_SEARCH_RESULT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  GB_WIDGET_CLASS_TEMPLATE (widget_class, "gb-search-display-row.ui");
  GB_WIDGET_CLASS_BIND (widget_class, GbSearchDisplayRow, progress);
  GB_WIDGET_CLASS_BIND (widget_class, GbSearchDisplayRow, subtitle);
  GB_WIDGET_CLASS_BIND (widget_class, GbSearchDisplayRow, title);
}

static void
gb_search_display_row_init (GbSearchDisplayRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
