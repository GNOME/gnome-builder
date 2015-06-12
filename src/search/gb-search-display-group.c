/* gb-search-display-group.c
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

#include "gb-search-display-group.h"
#include "gb-search-display-row.h"
#include "gb-widget.h"

struct _GbSearchDisplayGroup
{
  GtkBox             parent_instance;

  /* References owned by instance */
  IdeSearchProvider *provider;

  /* References owned by template */
  GtkLabel          *more_label;
  GtkListBoxRow     *more_row;
  GtkLabel          *label;
  GtkListBox        *rows;

  guint64            count;
};

G_DEFINE_TYPE (GbSearchDisplayGroup, gb_search_display_group, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_PROVIDER,
  PROP_SIZE_GROUP,
  LAST_PROP
};

enum {
  RESULT_ACTIVATED,
  RESULT_SELECTED,
  LAST_SIGNAL
};

static GQuark      gQuarkRow;
static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

IdeSearchResult *
gb_search_display_group_get_first (GbSearchDisplayGroup *self)
{
  GtkListBoxRow *row;

  g_return_val_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (self), NULL);

  row = gtk_list_box_get_row_at_y (self->rows, 1);

  if (row)
    {
      GtkWidget *child;

      child = gtk_bin_get_child (GTK_BIN (row));
      if (GB_IS_SEARCH_DISPLAY_ROW (child))
        return gb_search_display_row_get_result (GB_SEARCH_DISPLAY_ROW (child));
    }

  return NULL;
}

IdeSearchProvider *
gb_search_display_group_get_provider (GbSearchDisplayGroup *self)
{
  g_return_val_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (self), NULL);

  return self->provider;
}

static void
gb_search_display_group_set_provider (GbSearchDisplayGroup *self,
                                      IdeSearchProvider    *provider)
{
  const gchar *verb;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (self));
  g_return_if_fail (!provider || IDE_IS_SEARCH_PROVIDER (provider));

  if (provider)
    {
      self->provider = g_object_ref (provider);
      verb = ide_search_provider_get_verb (provider);
      gtk_label_set_label (self->label, verb);
    }
}

static void
gb_search_display_group_set_size_group (GbSearchDisplayGroup *self,
                                        GtkSizeGroup         *size_group)
{
  g_return_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (self));
  g_return_if_fail (!size_group || GTK_IS_SIZE_GROUP (size_group));

  if (size_group)
    gtk_size_group_add_widget (size_group, GTK_WIDGET (self->label));
}

GtkWidget *
gb_search_display_group_create_row (IdeSearchResult *result)
{
  IdeSearchProvider *provider;
  GtkWidget *row;

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (result), NULL);

  provider = ide_search_result_get_provider (result);
  row = ide_search_provider_create_row (provider, result);
  g_object_set_qdata (G_OBJECT (result), gQuarkRow, row);

  return row;
}

void
gb_search_display_group_remove_result (GbSearchDisplayGroup *self,
                                       IdeSearchResult      *result)
{
  GtkWidget *row;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (self));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

  row = g_object_get_qdata (G_OBJECT (result), gQuarkRow);

  if (row)
    gtk_container_remove (GTK_CONTAINER (self->rows), row);
}

void
gb_search_display_group_add_result (GbSearchDisplayGroup *self,
                                    IdeSearchResult      *result)
{
  GtkWidget *row;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (self));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

  row = gb_search_display_group_create_row (result);
  gtk_container_add (GTK_CONTAINER (self->rows), row);

  gtk_list_box_invalidate_sort (self->rows);

  self->count++;
}

void
gb_search_display_group_set_count (GbSearchDisplayGroup *self,
                                   guint64               count)
{
  GtkWidget *parent;
  gchar *count_str;
  gchar *markup;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (self));

  count_str = g_strdup_printf ("%"G_GUINT64_FORMAT, count);
  markup = g_strdup_printf (_("%s more"), count_str);
  gtk_label_set_label (self->more_label, markup);
  g_free (markup);
  g_free (count_str);

  parent = GTK_WIDGET (self->more_row);

  if ((count - self->count) > 0)
    gtk_widget_show (parent);
  else
    gtk_widget_hide (parent);
}

static gint
compare_cb (GtkListBoxRow *row1,
            GtkListBoxRow *row2,
            gpointer       user_data)
{
  GtkListBoxRow *more_row = user_data;
  IdeSearchResult *result1;
  IdeSearchResult *result2;
  gfloat score1;
  gfloat score2;

  if (row1 == more_row)
    return 1;
  else if (row2 == more_row)
    return -1;

  result1 = gb_search_display_row_get_result (GB_SEARCH_DISPLAY_ROW (row1));
  result2 = gb_search_display_row_get_result (GB_SEARCH_DISPLAY_ROW (row2));

  score1 = ide_search_result_get_score (result1);
  score2 = ide_search_result_get_score (result2);

  if (score1 < score2)
    return 1;
  else if (score1 > score2)
    return -1;
  else
    return 0;
}

static void
gb_search_display_group_result_activated (GbSearchDisplayGroup *self,
                                          GtkWidget            *widget,
                                          IdeSearchResult      *result)
{
  IdeSearchProvider *provider;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

  provider = ide_search_result_get_provider (result);
  ide_search_provider_activate (provider, widget, result);
}

void
gb_search_display_group_unselect (GbSearchDisplayGroup *self)
{
  g_return_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (self));

  gtk_list_box_unselect_all (self->rows);
}

static void
gb_search_display_group_row_activated (GbSearchDisplayGroup *self,
                                       GtkListBoxRow        *row,
                                       GtkListBox           *list_box)
{
  IdeSearchResult *result;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (self));
  g_return_if_fail (GB_IS_SEARCH_DISPLAY_ROW (row));
  g_return_if_fail (GTK_IS_LIST_BOX (list_box));

  result = gb_search_display_row_get_result (GB_SEARCH_DISPLAY_ROW (row));
  if (result)
    g_signal_emit (self, gSignals [RESULT_ACTIVATED], 0, row, result);
}

static void
gb_search_display_group_row_selected (GbSearchDisplayGroup *self,
                                      GtkListBoxRow        *row,
                                      GtkListBox           *list_box)
{
  GtkWidget *child;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (self));
  g_return_if_fail (!row || GTK_IS_LIST_BOX_ROW (row));
  g_return_if_fail (GTK_IS_LIST_BOX (list_box));

  if (row)
    {
      child = gtk_bin_get_child (GTK_BIN (row));

      if (GB_IS_SEARCH_DISPLAY_ROW (child))
        {
          IdeSearchResult *result;

          result = gb_search_display_row_get_result (GB_SEARCH_DISPLAY_ROW (child));
          if (result)
            g_signal_emit (self, gSignals [RESULT_SELECTED], 0, result);
        }
    }
}

void
gb_search_display_group_focus_first (GbSearchDisplayGroup *self)
{
  GtkListBoxRow *row;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (self));

  row = gtk_list_box_get_row_at_y (self->rows, 1);

  if (row)
    {
      gtk_list_box_unselect_all (self->rows);
      gtk_widget_child_focus (GTK_WIDGET (self->rows), GTK_DIR_DOWN);
    }
}

void
gb_search_display_group_focus_last (GbSearchDisplayGroup *self)
{
  GtkAllocation alloc;
  GtkListBoxRow *row;

  g_return_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (self));

  gtk_widget_get_allocation (GTK_WIDGET (self->rows), &alloc);
  row = gtk_list_box_get_row_at_y (self->rows, alloc.height - 2);

  if (row)
    {
      gtk_list_box_unselect_all (self->rows);
      gtk_widget_child_focus (GTK_WIDGET (self->rows), GTK_DIR_UP);
    }
}

static void
gb_search_display_group_header_cb (GtkListBoxRow *row,
                                   GtkListBoxRow *before,
                                   gpointer       user_data)
{
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (row));

  if (row)
    {
      GtkWidget *header;

      header = g_object_new (GTK_TYPE_SEPARATOR,
                             "orientation", GTK_ORIENTATION_HORIZONTAL,
                             "visible", TRUE,
                             NULL);
      gtk_list_box_row_set_header (row, header);
    }
}

static gboolean
gb_search_display_group_keynav_failed (GbSearchDisplayGroup *self,
                                       GtkDirectionType      dir,
                                       GtkListBox           *list_box)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (GB_IS_SEARCH_DISPLAY_GROUP (self), FALSE);
  g_return_val_if_fail (GTK_IS_LIST_BOX (list_box), FALSE);

  g_signal_emit_by_name (self, "keynav-failed", dir, &ret);

  return ret;
}

static void
gb_search_display_group_finalize (GObject *object)
{
  GbSearchDisplayGroup *self = (GbSearchDisplayGroup *)object;

  g_clear_object (&self->provider);

  G_OBJECT_CLASS (gb_search_display_group_parent_class)->finalize (object);
}

static void
gb_search_display_group_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GbSearchDisplayGroup *self = GB_SEARCH_DISPLAY_GROUP (object);

  switch (prop_id)
    {
    case PROP_PROVIDER:
      g_value_set_object (value, gb_search_display_group_get_provider (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_display_group_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GbSearchDisplayGroup *self = GB_SEARCH_DISPLAY_GROUP (object);

  switch (prop_id)
    {
    case PROP_PROVIDER:
      gb_search_display_group_set_provider (self, g_value_get_object (value));
      break;

    case PROP_SIZE_GROUP:
      gb_search_display_group_set_size_group (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_display_group_class_init (GbSearchDisplayGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_search_display_group_finalize;
  object_class->get_property = gb_search_display_group_get_property;
  object_class->set_property = gb_search_display_group_set_property;

  gParamSpecs [PROP_PROVIDER] =
    g_param_spec_object ("provider",
                         _("Provider"),
                         _("The search provider"),
                         IDE_TYPE_SEARCH_PROVIDER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_SIZE_GROUP] =
    g_param_spec_object ("size-group",
                         _("Size Group"),
                         _("The size group for the label."),
                         GTK_TYPE_SIZE_GROUP,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  gSignals [RESULT_ACTIVATED] =
    g_signal_new_class_handler ("result-activated",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (gb_search_display_group_result_activated),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                2,
                                GTK_TYPE_WIDGET,
                                IDE_TYPE_SEARCH_RESULT);

  gSignals [RESULT_SELECTED] =
    g_signal_new ("result-selected",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_SEARCH_RESULT);

  GB_WIDGET_CLASS_TEMPLATE (widget_class, "gb-search-display-group.ui");
  GB_WIDGET_CLASS_BIND (widget_class, GbSearchDisplayGroup, more_label);
  GB_WIDGET_CLASS_BIND (widget_class, GbSearchDisplayGroup, more_row);
  GB_WIDGET_CLASS_BIND (widget_class, GbSearchDisplayGroup, label);
  GB_WIDGET_CLASS_BIND (widget_class, GbSearchDisplayGroup, rows);

  gQuarkRow = g_quark_from_static_string ("GB_SEARCH_DISPLAY_ROW");
}

static void
gb_search_display_group_init (GbSearchDisplayGroup *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->rows,
                           "keynav-failed",
                           G_CALLBACK (gb_search_display_group_keynav_failed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->rows,
                           "row-activated",
                           G_CALLBACK (gb_search_display_group_row_activated),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->rows,
                           "row-selected",
                           G_CALLBACK (gb_search_display_group_row_selected),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_list_box_set_sort_func (self->rows, compare_cb, self->more_row, NULL);
  gtk_list_box_set_header_func (self->rows, gb_search_display_group_header_cb, NULL, NULL);
}
