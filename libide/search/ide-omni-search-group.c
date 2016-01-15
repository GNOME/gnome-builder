/* ide-omni-search-display-group.c
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

#include "ide-omni-search-group.h"
#include "ide-omni-search-row.h"

struct _IdeOmniSearchGroup
{
  GtkBox             parent_instance;

  /* References owned by instance */
  IdeSearchProvider *provider;

  /* References owned by template */
  GtkListBox        *rows;

  guint64            count;
};

G_DEFINE_TYPE (IdeOmniSearchGroup, ide_omni_search_group, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_PROVIDER,
  LAST_PROP
};

enum {
  RESULT_ACTIVATED,
  RESULT_SELECTED,
  LAST_SIGNAL
};

static GQuark      quarkRow;
static GParamSpec *properties [LAST_PROP];
static guint       signals [LAST_SIGNAL];

static void
ide_omni_search_group_foreach_cb (GtkWidget *widget,
                                  gpointer   user_data)
{
  GtkWidget **row = user_data;

  if (*row == NULL)
    *row = widget;
}

IdeSearchResult *
ide_omni_search_group_get_first (IdeOmniSearchGroup *self)
{
  GtkListBoxRow *row = NULL;
  IdeSearchResult *ret = NULL;

  g_return_val_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self), NULL);

  gtk_container_foreach (GTK_CONTAINER (self->rows),
                         ide_omni_search_group_foreach_cb,
                         &row);

  if (IDE_IS_OMNI_SEARCH_ROW (row))
    ret = ide_omni_search_row_get_result (IDE_OMNI_SEARCH_ROW (row));

  return ret;
}

IdeSearchProvider *
ide_omni_search_group_get_provider (IdeOmniSearchGroup *self)
{
  g_return_val_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self), NULL);

  return self->provider;
}

static void
ide_omni_search_group_set_provider (IdeOmniSearchGroup *self,
                                    IdeSearchProvider  *provider)
{
  g_return_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self));
  g_return_if_fail (!provider || IDE_IS_SEARCH_PROVIDER (provider));

  if (provider)
    self->provider = g_object_ref (provider);
}

GtkWidget *
ide_omni_search_group_create_row (IdeSearchResult *result)
{
  IdeSearchProvider *provider;
  GtkWidget *row;

  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (result), NULL);

  provider = ide_search_result_get_provider (result);
  row = ide_search_provider_create_row (provider, result);
  g_object_set_qdata (G_OBJECT (result), quarkRow, row);

  return row;
}

void
ide_omni_search_group_remove_result (IdeOmniSearchGroup *self,
                                     IdeSearchResult    *result)
{
  GtkWidget *row;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

  row = g_object_get_qdata (G_OBJECT (result), quarkRow);

  if (row)
    gtk_container_remove (GTK_CONTAINER (self->rows), row);
}

void
ide_omni_search_group_add_result (IdeOmniSearchGroup *self,
                                  IdeSearchResult    *result)
{
  GtkWidget *row;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

  row = ide_omni_search_group_create_row (result);
  gtk_container_add (GTK_CONTAINER (self->rows), row);

  gtk_list_box_invalidate_sort (self->rows);

  self->count++;
}

static gint
compare_cb (GtkListBoxRow *row1,
            GtkListBoxRow *row2,
            gpointer       user_data)
{
  IdeSearchResult *result1;
  IdeSearchResult *result2;
  gfloat score1;
  gfloat score2;

  result1 = ide_omni_search_row_get_result (IDE_OMNI_SEARCH_ROW (row1));
  result2 = ide_omni_search_row_get_result (IDE_OMNI_SEARCH_ROW (row2));

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
ide_omni_search_group_result_activated (IdeOmniSearchGroup *self,
                                        GtkWidget          *widget,
                                        IdeSearchResult    *result)
{
  IdeSearchProvider *provider;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

  provider = ide_search_result_get_provider (result);
  ide_search_provider_activate (provider, widget, result);
}

void
ide_omni_search_group_unselect (IdeOmniSearchGroup *self)
{
  g_return_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self));

  gtk_list_box_unselect_all (self->rows);
}

static void
ide_omni_search_group_row_activated (IdeOmniSearchGroup *self,
                                     GtkListBoxRow      *row,
                                     GtkListBox         *list_box)
{
  IdeSearchResult *result;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self));
  g_return_if_fail (IDE_IS_OMNI_SEARCH_ROW (row));
  g_return_if_fail (GTK_IS_LIST_BOX (list_box));

  result = ide_omni_search_row_get_result (IDE_OMNI_SEARCH_ROW (row));
  if (result)
    g_signal_emit (self, signals [RESULT_ACTIVATED], 0, row, result);
}

static void
ide_omni_search_group_row_selected (IdeOmniSearchGroup *self,
                                    GtkListBoxRow      *row,
                                    GtkListBox         *list_box)
{
  GtkWidget *child;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self));
  g_return_if_fail (!row || GTK_IS_LIST_BOX_ROW (row));
  g_return_if_fail (GTK_IS_LIST_BOX (list_box));

  if (row)
    {
      child = gtk_bin_get_child (GTK_BIN (row));

      if (IDE_IS_OMNI_SEARCH_ROW (child))
        {
          IdeSearchResult *result;

          result = ide_omni_search_row_get_result (IDE_OMNI_SEARCH_ROW (child));
          if (result)
            g_signal_emit (self, signals [RESULT_SELECTED], 0, result);
        }
    }
}

void
ide_omni_search_group_select_first (IdeOmniSearchGroup *self)
{
  GtkListBoxRow *row;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self));

  row = gtk_list_box_get_row_at_index (self->rows, 0);

  if (row)
    {
      gtk_list_box_unselect_all (self->rows);
      gtk_list_box_select_row (self->rows, row);
    }
}

void
ide_omni_search_group_select_last (IdeOmniSearchGroup *self)
{
  GtkAllocation alloc;
  GtkListBoxRow *row;

  g_return_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self));

  gtk_widget_get_allocation (GTK_WIDGET (self->rows), &alloc);
  row = gtk_list_box_get_row_at_y (self->rows, alloc.height - 2);

  if (row)
    {
      gtk_list_box_unselect_all (self->rows);
      gtk_widget_child_focus (GTK_WIDGET (self->rows), GTK_DIR_UP);
    }
}

static gboolean
ide_omni_search_group_keynav_failed (IdeOmniSearchGroup *self,
                                     GtkDirectionType    dir,
                                     GtkListBox         *list_box)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self), FALSE);
  g_return_val_if_fail (GTK_IS_LIST_BOX (list_box), FALSE);

  g_signal_emit_by_name (self, "keynav-failed", dir, &ret);

  return ret;
}

static void
ide_omni_search_group_finalize (GObject *object)
{
  IdeOmniSearchGroup *self = (IdeOmniSearchGroup *)object;

  g_clear_object (&self->provider);

  G_OBJECT_CLASS (ide_omni_search_group_parent_class)->finalize (object);
}

static void
ide_omni_search_group_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeOmniSearchGroup *self = IDE_OMNI_SEARCH_GROUP (object);

  switch (prop_id)
    {
    case PROP_PROVIDER:
      g_value_set_object (value, ide_omni_search_group_get_provider (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_omni_search_group_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeOmniSearchGroup *self = IDE_OMNI_SEARCH_GROUP (object);

  switch (prop_id)
    {
    case PROP_PROVIDER:
      ide_omni_search_group_set_provider (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_omni_search_group_class_init (IdeOmniSearchGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_omni_search_group_finalize;
  object_class->get_property = ide_omni_search_group_get_property;
  object_class->set_property = ide_omni_search_group_set_property;

  properties [PROP_PROVIDER] =
    g_param_spec_object ("provider",
                         "Provider",
                         "The search provider",
                         IDE_TYPE_SEARCH_PROVIDER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [RESULT_ACTIVATED] =
    g_signal_new_class_handler ("result-activated",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_omni_search_group_result_activated),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                2,
                                GTK_TYPE_WIDGET,
                                IDE_TYPE_SEARCH_RESULT);

  signals [RESULT_SELECTED] =
    g_signal_new ("result-selected",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_SEARCH_RESULT);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-omni-search-group.ui");
  gtk_widget_class_set_css_name (widget_class, "omnisearchgroup");
  gtk_widget_class_bind_template_child (widget_class, IdeOmniSearchGroup, rows);

  quarkRow = g_quark_from_static_string ("IDE_OMNI_SEARCH_ROW");
}

static void
ide_omni_search_group_init (IdeOmniSearchGroup *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->rows,
                           "keynav-failed",
                           G_CALLBACK (ide_omni_search_group_keynav_failed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->rows,
                           "row-activated",
                           G_CALLBACK (ide_omni_search_group_row_activated),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->rows,
                           "row-selected",
                           G_CALLBACK (ide_omni_search_group_row_selected),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_list_box_set_sort_func (self->rows, compare_cb, NULL, NULL);
}

gboolean
ide_omni_search_group_activate (IdeOmniSearchGroup *group)
{
  GtkListBoxRow *row;

  g_return_val_if_fail (IDE_IS_OMNI_SEARCH_GROUP (group), FALSE);

  row = gtk_list_box_get_selected_row (group->rows);

  if (row != NULL)
    {
      IdeSearchResult *result;
      IdeSearchProvider *provider;

      g_assert (IDE_IS_OMNI_SEARCH_ROW (row));

      result = ide_omni_search_row_get_result (IDE_OMNI_SEARCH_ROW (row));
      provider = ide_search_result_get_provider (result);
      ide_search_provider_activate (provider, GTK_WIDGET (row), result);

      return TRUE;
    }

  return FALSE;
}

guint64
ide_omni_search_group_get_count (IdeOmniSearchGroup *self)
{
  g_return_val_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self), 0);

  return self->count;
}

static void
find_nth_row_cb (GtkWidget *widget,
                 gpointer   user_data)
{
  struct {
    GtkListBox    *list;
    GtkListBoxRow *row;
    gint           nth;
    gint           last_n;
  } *lookup = user_data;
  gint position;

  /*
   * This tries to find a matching index, but also handles the special case of
   * -1, where we are trying to find the last row by position. This would not
   *  be very efficient if the lists were long, but currently the lists are
   *  all < 10 items.
   */

  if ((lookup->row != NULL) && (lookup->nth != -1))
    return;

  position = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (widget));

  if (position == lookup->nth)
    lookup->row = GTK_LIST_BOX_ROW (widget);
  else if ((lookup->nth == -1) && (position > lookup->last_n))
    {
      lookup->row = GTK_LIST_BOX_ROW (widget);
      lookup->last_n = position;
    }
}

static GtkListBoxRow *
find_nth_row (GtkListBox *list,
              gint        nth)
{
  struct {
    GtkListBox    *list;
    GtkListBoxRow *row;
    gint           nth;
    gint           last_n;
  } lookup = { list, NULL, nth, -1 };

  g_assert (GTK_IS_LIST_BOX (list));
  g_assert (nth >= -1);

  gtk_container_foreach (GTK_CONTAINER (list), find_nth_row_cb, &lookup);

  return lookup.row;
}

gboolean
ide_omni_search_group_move_next (IdeOmniSearchGroup *self)
{
  GtkListBoxRow *row;

  g_return_val_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self), FALSE);

  row = gtk_list_box_get_selected_row (self->rows);

  if (row != NULL)
    {
      gint position;

      position = gtk_list_box_row_get_index (row);
      row = find_nth_row (self->rows, position + 1);
    }
  else
    row = find_nth_row (self->rows, 0);

  if (row != NULL)
    {
      gtk_list_box_select_row (self->rows, row);
      return TRUE;
    }

  return FALSE;
}

gboolean
ide_omni_search_group_move_previous (IdeOmniSearchGroup *self)
{
  GtkListBoxRow *row;

  g_return_val_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self), FALSE);

  row = gtk_list_box_get_selected_row (self->rows);

  if (row != NULL)
    {
      gint position;

      position = gtk_list_box_row_get_index (row);

      if (position == 0)
        return FALSE;

      row = find_nth_row (self->rows, position - 1);
    }
  else
    row = find_nth_row (self->rows, -1);

  if (row != NULL)
    {
      gtk_list_box_select_row (self->rows, row);
      return TRUE;
    }

  return FALSE;
}

gboolean
ide_omni_search_group_has_selection (IdeOmniSearchGroup *self)
{
  g_return_val_if_fail (IDE_IS_OMNI_SEARCH_GROUP (self), FALSE);

  return !!gtk_list_box_get_selected_row (self->rows);
}
