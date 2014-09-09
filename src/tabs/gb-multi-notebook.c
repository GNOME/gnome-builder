/* gb-multi-notebook.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "multi-notebook"

#include <glib/gi18n.h>

#include "gb-animation.h"
#include "gb-box-theatric.h"
#include "gb-log.h"
#include "gb-multi-notebook.h"
#include "gb-tab-label.h"

struct _GbMultiNotebookPrivate
{
  GtkPaned      *root_paned;
  GbNotebook    *active_notebook;
  GbTab         *active_tab;

  GbBoxTheatric *theatric;
  GbAnimation   *theatric_anim;

  GPtrArray     *notebooks;

  gchar         *group_name;

  GbNotebook    *drag_drop_target;
  gint           drag_drop_position;

  guint          show_tabs : 1;
};

enum {
  PROP_0,
  PROP_ACTIVE_NOTEBOOK,
  PROP_ACTIVE_TAB,
  PROP_GROUP_NAME,
  PROP_N_NOTEBOOKS,
  PROP_SHOW_TABS,
  LAST_PROP
};

enum {
  CREATE_WINDOW,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_PRIVATE (GbMultiNotebook, gb_multi_notebook, GTK_TYPE_GRID)

static GParamSpec * gParamSpecs[LAST_PROP];
static guint gSignals[LAST_SIGNAL];

gint
_g_ptr_array_find (GPtrArray *array,
                   gpointer   data)
{
  guint i;

  for (i = 0; i < array->len; i++)
    if (g_ptr_array_index (array, i) == data)
      return i;

  return -1;
}

GtkWidget *
gb_multi_notebook_new (void)
{
  return g_object_new (GB_TYPE_MULTI_NOTEBOOK, NULL);
}

gboolean
gb_multi_notebook_get_show_tabs (GbMultiNotebook *self)
{
  g_return_val_if_fail (GB_IS_MULTI_NOTEBOOK (self), FALSE);

  return self->priv->show_tabs;
}

void
gb_multi_notebook_set_show_tabs (GbMultiNotebook *self,
                                 gboolean         show_tabs)
{
  GbMultiNotebookPrivate *priv;
  GtkNotebook *notebook;
  guint i;

  g_return_if_fail (GB_IS_MULTI_NOTEBOOK (self));

  priv = self->priv;

  priv->show_tabs = show_tabs;

  for (i = 0; i < priv->notebooks->len; i++)
    {
      notebook = g_ptr_array_index (priv->notebooks, i);
      gtk_notebook_set_show_tabs (notebook, show_tabs);
    }

  g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs[PROP_SHOW_TABS]);
}

guint
gb_multi_notebook_get_n_notebooks (GbMultiNotebook *self)
{
  g_return_val_if_fail (GB_IS_MULTI_NOTEBOOK (self), 0);

  return self->priv->notebooks->len;
}

const gchar *
gb_multi_notebook_get_group_name (GbMultiNotebook *notebook)
{
  g_return_val_if_fail (GB_IS_MULTI_NOTEBOOK (notebook), NULL);

  return notebook->priv->group_name;
}

void
gb_multi_notebook_set_group_name (GbMultiNotebook *notebook,
                                  const gchar     *group_name)
{
  GtkWidget *child;
  guint i;

  g_return_if_fail (GB_IS_MULTI_NOTEBOOK (notebook));

  g_free (notebook->priv->group_name);
  notebook->priv->group_name = g_strdup (group_name);

  for (i = 0; i < notebook->priv->notebooks->len; i++)
    {
      child = g_ptr_array_index (notebook->priv->notebooks, i);
      gtk_notebook_set_group_name (GTK_NOTEBOOK (child), group_name);
    }

  g_object_notify_by_pspec (G_OBJECT (notebook),
                            gParamSpecs[PROP_GROUP_NAME]);
}

static void
gb_multi_notebook_set_active_notebook (GbMultiNotebook *mnb,
                                       GbNotebook      *notebook)
{
  GbMultiNotebookPrivate *priv;

  g_return_if_fail (GB_IS_MULTI_NOTEBOOK (mnb));
  g_return_if_fail (!notebook || GB_IS_NOTEBOOK (notebook));

  priv = mnb->priv;

  if (notebook != priv->active_notebook)
    {
      if (priv->active_notebook)
        {
          g_object_remove_weak_pointer (G_OBJECT (priv->active_notebook),
                                        (gpointer *) &priv->active_notebook);
          priv->active_notebook = NULL;
        }

      if (notebook)
        {
          priv->active_notebook = notebook;
          g_object_add_weak_pointer (G_OBJECT (notebook),
                                     (gpointer *) &priv->active_notebook);
        }

      g_object_notify_by_pspec (G_OBJECT (mnb),
                                gParamSpecs[PROP_ACTIVE_NOTEBOOK]);
    }
}

GbNotebook *
gb_multi_notebook_get_active_notebook (GbMultiNotebook *self)
{
  g_return_val_if_fail (GB_IS_MULTI_NOTEBOOK (self), NULL);

  return self->priv->active_notebook;
}

GbTab *
gb_multi_notebook_get_active_tab (GbMultiNotebook *self)
{
  g_return_val_if_fail (GB_IS_MULTI_NOTEBOOK (self), NULL);

  return self->priv->active_tab;
}

static void
gb_multi_notebook_set_active_tab (GbMultiNotebook *self,
                                  GbTab           *tab)
{
  GbMultiNotebookPrivate *priv;

  g_return_if_fail (GB_IS_MULTI_NOTEBOOK (self));

  priv = self->priv;

  if (tab != priv->active_tab)
    {
      if (priv->active_tab)
        {
          g_object_remove_weak_pointer (G_OBJECT (priv->active_tab),
                                        (gpointer *) &priv->active_tab);
          priv->active_tab = NULL;
        }

      if (tab)
        {
          priv->active_tab = tab;
          g_object_add_weak_pointer (G_OBJECT (tab),
                                     (gpointer *) &priv->active_tab);
        }

      g_object_notify_by_pspec (G_OBJECT (self),
                                gParamSpecs[PROP_ACTIVE_TAB]);
    }
}

GList *
gb_multi_notebook_get_all_tabs (GbMultiNotebook *self)
{
  GbMultiNotebookPrivate *priv;
  GbNotebook *notebook;
  GList *list = NULL;
  GbTab *tab;
  guint n_pages;
  guint i;
  guint j;

  g_return_val_if_fail (GB_IS_MULTI_NOTEBOOK (self), NULL);

  priv = self->priv;

  for (i = 0; i < priv->notebooks->len; i++)
    {
      notebook = g_ptr_array_index (priv->notebooks, i);
      n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
      for (j = 0; j < n_pages; j++)
        {
          tab = GB_TAB (gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), j));
          list = g_list_prepend (list, tab);
        }
    }

  return g_list_reverse (list);
}

static void
resize_paned_positions (GbMultiNotebook *mnb)
{
  GbMultiNotebookPrivate *priv;
  GtkAllocation alloc;
  GtkWidget *child;
  GtkWidget *parent;
  guint position;
  guint i;

  g_return_if_fail (GB_IS_MULTI_NOTEBOOK (mnb));

  priv = mnb->priv;

  if (!priv->notebooks->len)
    return;

  gtk_widget_get_allocation (GTK_WIDGET (mnb), &alloc);
  position = alloc.width / priv->notebooks->len;

  for (i = 0; i < priv->notebooks->len; i++)
    {
      child = g_ptr_array_index (priv->notebooks, i);
      g_assert (GB_IS_NOTEBOOK (child));

      parent = gtk_widget_get_parent (child);
      g_assert (GTK_IS_PANED (parent));

      gtk_paned_set_position (GTK_PANED (parent), position);
    }
}

static void
remove_unused_notebooks (GbMultiNotebook *mnb)
{
  GbMultiNotebookPrivate *priv;
  gboolean at_least_one = FALSE;
  gboolean has_unused = FALSE;
  GSList *list = NULL;
  GSList *iter;
  guint i;

  ENTRY;

  g_assert (GB_IS_MULTI_NOTEBOOK (mnb));

  priv = mnb->priv;

  /*
   * Find all candidate notebooks for removal
   */
  for (i = 0; i < priv->notebooks->len; i++)
    {
      GbNotebook *notebook;

      notebook = g_ptr_array_index (priv->notebooks, i);
      g_assert (GB_IS_NOTEBOOK (notebook));

      if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)))
        at_least_one = TRUE;
      else
        list = g_slist_prepend (list, g_object_ref (notebook));
    }

  /*
   * If we are trying to remove all of the notebooks, then ignore removal
   * of the first notebook.
   */
  if (!at_least_one)
    {
      g_assert (list);

      g_object_unref (list->data);
      list = g_slist_remove (list, list);
    }

  has_unused = !!list;

  /*
   * Reparent notebooks, removing the targets from the chain.
   */
  for (iter = list; iter; iter = iter->next)
    {
      GbNotebook *notebook;
      GtkPaned *container_parent;
      GtkPaned *container;
      GtkPaned *child;

      notebook = iter->data;
      g_assert (GB_IS_NOTEBOOK (notebook));

      container = GTK_PANED (gtk_widget_get_parent (GTK_WIDGET (notebook)));
      g_assert (GTK_IS_PANED (container));

      container_parent = GTK_PANED (gtk_widget_get_parent (GTK_WIDGET (container)));
      g_assert (GTK_IS_PANED (container_parent));

      child = (GtkPaned *) gtk_paned_get_child2 (GTK_PANED (container));
      g_assert (!child || GTK_IS_PANED (child));

      if (child)
        {
          g_object_ref (child);
          gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (child));
        }

      gtk_widget_destroy (GTK_WIDGET (container));

      if (child)
        {
          gtk_paned_add2 (container_parent, GTK_WIDGET (child));
          g_object_unref (child);
        }

      g_ptr_array_remove (priv->notebooks, notebook);
    }

  g_slist_foreach (list, (GFunc) g_object_unref, NULL);
  g_slist_free (list);

  /*
   * Adjust each notebook width to be width/n_notebooks.
   */
  if (priv->notebooks->len)
    {
      GtkAllocation alloc;
      guint position;

      gtk_widget_get_allocation (GTK_WIDGET (mnb), &alloc);

      if (alloc.x && alloc.y)
        {
          position = alloc.x / priv->notebooks->len;

          for (i = 0; i < priv->notebooks->len; i++)
            {
              GbNotebook *notebook;
              GtkPaned *paned;

              notebook = g_ptr_array_index (priv->notebooks, i);
              g_assert (GB_IS_NOTEBOOK (notebook));

              paned = GTK_PANED (gtk_widget_get_parent (GTK_WIDGET (notebook)));
              g_assert (GTK_IS_PANED (paned));

              gtk_paned_set_position (paned, position);
            }
        }
    }

  if (has_unused)
    {
      resize_paned_positions (mnb);
      g_object_notify_by_pspec (G_OBJECT (mnb),
                                gParamSpecs[PROP_N_NOTEBOOKS]);
    }

  EXIT;
}

static gint
get_drop_position (GtkAllocation *alloc,
                   gint           x,
                   gint           y)
{
  if (x < (alloc->width / 3.0))
    return -1;
  else if (x > ((alloc->width / 3.0) * 2.0))
    return 1;
  else
    return 0;
}

static void
on_drag_begin (GbNotebook      *notebook,
               GdkDragContext  *context,
               GbMultiNotebook *mnb)
{
  GList *list;
  GList *iter;

  ENTRY;

  g_return_if_fail (GB_IS_NOTEBOOK (notebook));
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (GB_IS_MULTI_NOTEBOOK (mnb));

  list = gb_multi_notebook_get_all_tabs (mnb);

  for (iter = list; iter; iter = iter->next)
    {
      g_assert (GB_IS_TAB (iter->data));
      gb_tab_freeze_drag (iter->data);
    }

  g_list_free (list);
}

static void
force_redraw (GbMultiNotebook *mnb)
{
  GbMultiNotebookPrivate *priv = mnb->priv;
  GtkWidget *widget;
  guint i;

  for (i = 0; i < priv->notebooks->len; i++)
    {
      widget = g_ptr_array_index (priv->notebooks, i);
      gtk_widget_queue_draw (widget);
    }

  gtk_widget_queue_draw (GTK_WIDGET (mnb));
}

static void
get_drag_drop_target_area (GbMultiNotebook *mnb,
                           GdkRectangle    *area)
{
  GbMultiNotebookPrivate *priv;
  GtkAllocation alloc;

  g_assert (GB_IS_MULTI_NOTEBOOK (mnb));
  g_assert (area);

  priv = mnb->priv;

  gtk_widget_get_allocation (GTK_WIDGET (priv->drag_drop_target), &alloc);
  gtk_widget_translate_coordinates (GTK_WIDGET (priv->drag_drop_target), GTK_WIDGET (mnb), alloc.x, alloc.y, &alloc.x, &alloc.y);

  if (priv->drag_drop_position == -1)
    {
      guint x1;
      guint x2;
      gint i;

      x1 = alloc.x;
      x2 = alloc.x + (alloc.width / 2.0);

      i = _g_ptr_array_find (priv->notebooks, priv->drag_drop_target);
      g_assert (i >= 0);

      if (i > 0)
        {
          GbNotebook *item;
          GtkAllocation item_alloc;

          item = g_ptr_array_index (priv->notebooks, i - 1);
          gtk_widget_get_allocation (GTK_WIDGET (item), &item_alloc);

          if (item_alloc.width < alloc.width)
            alloc.width = item_alloc.width;

          x1 = alloc.x - (alloc.width / 2);
          x2 = x1 + alloc.width;
        }

      alloc.x = x1;
      alloc.width = x2 - x1;
    }
  else if (priv->drag_drop_position == 1)
    {
      guint x1;
      guint x2;
      gint i;

      x1 = alloc.x + (alloc.width / 2.0);
      x2 = alloc.x + alloc.width;

      i = _g_ptr_array_find (priv->notebooks, priv->drag_drop_target);
      g_assert (i >= 0);

      if (i != (priv->notebooks->len - 1))
        {
          GbNotebook *item;
          GtkAllocation item_alloc;

          item = g_ptr_array_index (priv->notebooks, i + 1);
          gtk_widget_get_allocation (GTK_WIDGET (item), &item_alloc);

          if (item_alloc.width < alloc.width)
            {
              x1 = alloc.x + alloc.width - (item_alloc.width / 2.0);
              alloc.width = item_alloc.width;
            }

          x2 = x1 + alloc.width;
        }

      alloc.x = x1;
      alloc.width = x2 - x1;
    }

  *area = alloc;
}

static void
animate_highlight (GbMultiNotebook    *mnb,
                   const GdkRectangle *area)
{
  GbMultiNotebookPrivate *priv;
  GdkFrameClock *frame_clock;
  GbAnimation *anim;
  GtkWidget *toplevel;

  ENTRY;

  g_assert (GB_IS_MULTI_NOTEBOOK (mnb));
  g_assert (area);

  priv = mnb->priv;

  if (!(toplevel = gtk_widget_get_toplevel (GTK_WIDGET (mnb))))
    return;

  if ((anim = priv->theatric_anim))
    {
      g_object_remove_weak_pointer (G_OBJECT (anim),
                                    (gpointer *) &priv->theatric_anim);
      priv->theatric_anim = NULL;
      gb_animation_stop (anim);
    }

  if (!priv->theatric)
    {
      priv->theatric = g_object_new (GB_TYPE_BOX_THEATRIC,
                                     "background", "#729fcf",
                                     "alpha", 0.0,
                                     "x", area->x,
                                     "y", area->y,
                                     "width", area->width,
                                     "height", area->height,
                                     "target", mnb,
                                     NULL);
    }

  frame_clock = gtk_widget_get_frame_clock (toplevel);

  priv->theatric_anim = gb_object_animate (priv->theatric,
                                           GB_ANIMATION_EASE_OUT_CUBIC,
                                           250,
                                           frame_clock,
                                           "x", area->x,
                                           "y", area->y,
                                           "width", area->width,
                                           "height", area->height,
                                           "alpha", 0.2,
                                           NULL);
  g_object_add_weak_pointer (G_OBJECT (priv->theatric_anim),
                             (gpointer *) &priv->theatric_anim);

  EXIT;
}

static gboolean
clear_theatric_timeout (gpointer data)
{
  GbMultiNotebookPrivate *priv;
  GbMultiNotebook *mnb = data;

  g_assert (GB_IS_MULTI_NOTEBOOK (mnb));

  priv = mnb->priv;

  if (!priv->drag_drop_target)
    {
      g_clear_object (&mnb->priv->theatric);
      force_redraw (mnb);
    }

  g_object_unref (mnb);

  return G_SOURCE_REMOVE;
}

static void
select_tab (GbNotebook *notebook,
            GbTab      *tab)
{
  gint position = -1;

  g_return_if_fail (GB_IS_NOTEBOOK (notebook));
  g_return_if_fail (GB_IS_TAB (tab));

  gtk_container_child_get (GTK_CONTAINER (notebook), GTK_WIDGET (tab),
                           "position", &position,
                           NULL);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), position);
}

static void
set_drag_drop_target (GbMultiNotebook *mnb,
                      GbNotebook      *notebook,
                      gint             position,
                      gboolean         animate)
{
  GbMultiNotebookPrivate *priv;
  GdkRectangle area;

  ENTRY;

  g_return_if_fail (GB_IS_MULTI_NOTEBOOK (mnb));
  g_return_if_fail (!notebook || GB_IS_NOTEBOOK (notebook));

  priv = mnb->priv;

  if ((notebook == priv->drag_drop_target) &&
      (position == priv->drag_drop_position))
    EXIT;

  if (priv->drag_drop_target)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->drag_drop_target),
                                    (gpointer *) &priv->drag_drop_target);
      priv->drag_drop_target = NULL;
    }

  priv->drag_drop_target = notebook;

  if (priv->drag_drop_target)
    g_object_add_weak_pointer (G_OBJECT (priv->drag_drop_target),
                               (gpointer *) &priv->drag_drop_target);

  priv->drag_drop_position = position;

  if (priv->drag_drop_target)
    {
      if (animate)
        {
          get_drag_drop_target_area (mnb, &area);
          animate_highlight (mnb, &area);
        }
      else
        g_clear_object (&priv->theatric);
    }
  else
    /*
     * HACK: This is to help in situations where the item drags over a
     *       paned handle, thereby causing our highlight to flicker.
     *       Adding just a bit of delay catches most of those. Not
     *       elegant, but works pretty well.
     */
    g_timeout_add (100, clear_theatric_timeout, g_object_ref (mnb));

  force_redraw (mnb);

  EXIT;
}

static void
on_drag_end (GbNotebook      *notebook,
             GdkDragContext  *context,
             GbMultiNotebook *mnb)
{
  GList *list;
  GList *iter;

  ENTRY;

  g_return_if_fail (GB_IS_NOTEBOOK (notebook));
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (GB_IS_MULTI_NOTEBOOK (mnb));

  set_drag_drop_target (mnb, NULL, 0, TRUE);

  list = gb_multi_notebook_get_all_tabs (mnb);

  for (iter = list; iter; iter = iter->next)
    {
      g_assert (GB_IS_TAB (iter->data));
      gb_tab_thaw_drag (iter->data);
    }

  g_list_free (list);

  remove_unused_notebooks (mnb);
  force_redraw (mnb);

  EXIT;
}

static gboolean
on_drag_motion (GbNotebook      *notebook,
                GdkDragContext  *context,
                gint             x,
                gint             y,
                guint            time_,
                GbMultiNotebook *mnb)
{
  GtkAllocation alloc;
  gint drop_position;

  ENTRY;

  g_return_val_if_fail (GB_IS_NOTEBOOK (notebook), FALSE);
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), FALSE);
  g_return_val_if_fail (GB_IS_MULTI_NOTEBOOK (mnb), FALSE);

  gtk_widget_get_allocation (GTK_WIDGET (notebook), &alloc);
  drop_position = get_drop_position (&alloc, x, y);
  set_drag_drop_target (mnb, notebook, drop_position, TRUE);

  RETURN (FALSE);
}

static gboolean
on_drag_drop (GbNotebook      *notebook,
              GdkDragContext  *context,
              gint             x,
              gint             y,
              guint            time_,
              GbMultiNotebook *mnb)
{
  GtkAllocation alloc;
  GdkAtom target;
  GdkAtom tab_target;
  gint drop_position;

  ENTRY;

  g_return_val_if_fail (GB_IS_NOTEBOOK (notebook), FALSE);
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), FALSE);
  g_return_val_if_fail (GB_IS_MULTI_NOTEBOOK (mnb), FALSE);

  gtk_widget_get_allocation (GTK_WIDGET (notebook), &alloc);

  drop_position = get_drop_position (&alloc, x, y);
  set_drag_drop_target (mnb, notebook, drop_position, FALSE);

  target = gtk_drag_dest_find_target (GTK_WIDGET (notebook), context, NULL);
  tab_target = gdk_atom_intern_static_string ("GTK_NOTEBOOK_TAB");

  if (target == tab_target)
    {
      gtk_drag_get_data (GTK_WIDGET (notebook), context, target, time_);
      RETURN (TRUE);
    }

  RETURN (FALSE);
}

static void
on_drag_data_received (GbNotebook       *notebook,
                       GdkDragContext   *context,
                       gint              x,
                       gint              y,
                       GtkSelectionData *data,
                       guint             info,
                       guint             time_,
                       GbMultiNotebook  *mnb)
{
  GbMultiNotebookPrivate *priv;
  GbNotebook *new_notebook;
  GtkWidget *source_widget;
  GdkAtom notebook_target;
  GdkAtom target;
  GbTab **tabptr;
  GbTab *tab;
  gint idx;

  ENTRY;

  g_return_if_fail (GB_IS_NOTEBOOK (notebook));
  g_return_if_fail (GDK_IS_DRAG_CONTEXT (context));
  g_return_if_fail (GB_IS_MULTI_NOTEBOOK (mnb));

  priv = mnb->priv;

  source_widget = gtk_drag_get_source_widget (context);
  notebook_target = gdk_atom_intern_static_string ("GTK_NOTEBOOK_TAB");
  target = gtk_selection_data_get_target (data);

  if (source_widget && (target == notebook_target))
    {
      GtkWidget *parent;

      g_assert (priv->drag_drop_target);
      g_assert (GB_IS_NOTEBOOK (priv->drag_drop_target));

      tabptr = (GbTab * *) gtk_selection_data_get_data (data);
      g_assert (tabptr);

      tab = *tabptr;
      g_assert (GB_IS_TAB (tab));

      g_object_ref (tab);

      g_signal_stop_emission_by_name (notebook, "drag-data-received");

      gtk_drag_finish (context, TRUE, FALSE, time_);

      if ((parent = gtk_widget_get_parent (GTK_WIDGET (tab))))
        gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (tab));

      idx = _g_ptr_array_find (priv->notebooks, notebook);
      g_assert (idx != -1);

      if (priv->drag_drop_position == 0)
        {
          gb_notebook_add_tab (notebook, tab);
          select_tab (notebook, tab);
        }
      else if (priv->drag_drop_position == -1)
        {
          new_notebook = g_object_new (GB_TYPE_NOTEBOOK,
                                       "group-name", priv->group_name,
                                       "visible", TRUE,
                                       NULL);
          gb_multi_notebook_insert_notebook (mnb, new_notebook, idx);
          gb_notebook_add_tab (new_notebook, tab);
          select_tab (new_notebook, tab);
        }
      else if (priv->drag_drop_position == 1)
        {
          new_notebook = g_object_new (GB_TYPE_NOTEBOOK,
                                       "group-name", priv->group_name,
                                       "visible", TRUE,
                                       NULL);
          gb_multi_notebook_insert_notebook (mnb, new_notebook, idx + 1);
          gb_notebook_add_tab (new_notebook, tab);
          select_tab (new_notebook, tab);
        }
      else
        g_assert_not_reached ();

      g_object_unref (tab);
    }

  set_drag_drop_target (mnb, NULL, 0, TRUE);

  remove_unused_notebooks (mnb);

  force_redraw (mnb);

  EXIT;
}

static gboolean
on_drag_leave (GbNotebook      *notebook,
               GdkDragContext  *context,
               guint            time_,
               GbMultiNotebook *mnb)
{
  ENTRY;

  g_return_val_if_fail (GB_IS_NOTEBOOK (notebook), FALSE);
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), FALSE);
  g_return_val_if_fail (GB_IS_MULTI_NOTEBOOK (mnb), FALSE);

  set_drag_drop_target (mnb, NULL, 0, TRUE);

  RETURN (FALSE);
}

static void
on_switch_page (GtkNotebook     *notebook,
                GtkWidget       *page,
                guint            page_num,
                GbMultiNotebook *mnb)
{
  ENTRY;

  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));
  g_return_if_fail (GB_IS_MULTI_NOTEBOOK (mnb));

  gb_multi_notebook_set_active_notebook (mnb, GB_NOTEBOOK (notebook));
  gb_multi_notebook_set_active_tab (mnb, GB_TAB (page));

  EXIT;
}

static void
on_set_focus_child (GbNotebook      *notebook,
                    GtkWidget       *widget,
                    GbMultiNotebook *mnb)
{
  GbTab *tab = NULL;

  ENTRY;

  if (GB_IS_TAB (widget))
    tab = GB_TAB (widget);
  else if (GB_IS_TAB_LABEL (widget))
    {
      tab = g_object_get_data (G_OBJECT (widget), "GB_TAB");
      g_assert (GB_IS_TAB (tab));
    }

  if (tab)
    {
      gb_multi_notebook_set_active_notebook (mnb, notebook);
      gb_multi_notebook_set_active_tab (mnb, tab);
    }

  EXIT;
}

static GtkNotebook *
on_create_window (GtkNotebook     *notebook,
                  GtkWidget       *page,
                  gint             x,
                  gint             y,
                  GbMultiNotebook *mnb)
{
  GtkNotebook *ret = NULL;

  ENTRY;

  g_return_val_if_fail (GTK_IS_NOTEBOOK (notebook), NULL);
  g_return_val_if_fail (GB_IS_TAB (page), NULL);
  g_return_val_if_fail (GB_IS_MULTI_NOTEBOOK (mnb), NULL);

  g_signal_emit (mnb, gSignals[CREATE_WINDOW], 0,
                 notebook, page, x, y,
                 &ret);

  RETURN (ret);
}

static gboolean
remove_unused_notebooks_cb (gpointer data)
{
  GbMultiNotebook *mnb = data;

  g_return_val_if_fail (GB_IS_MULTI_NOTEBOOK (mnb), G_SOURCE_REMOVE);

  remove_unused_notebooks (mnb);

  return G_SOURCE_REMOVE;
}

static void
on_remove (GtkContainer    *container,
           GtkWidget       *widget,
           GbMultiNotebook *mnb)
{
  g_return_if_fail (GB_IS_MULTI_NOTEBOOK (mnb));
  g_return_if_fail (GB_IS_NOTEBOOK (container));

  /*
   * WORKAROUND:
   *
   * We delay the cleanup here of unused notebooks since we could be
   * in the process of dropping back onto ourselves. Probably just need
   * to avoid the drag'n'drop code altogether when that happens by
   * cancelling the drag upon drop.
   */
  g_timeout_add (0, remove_unused_notebooks_cb, g_object_ref (mnb));
}

static void
connect_notebook_signals (GbMultiNotebook *self,
                          GbNotebook      *notebook)
{
  g_assert (GB_IS_MULTI_NOTEBOOK (self));
  g_assert (GB_IS_NOTEBOOK (notebook));

  g_signal_connect (notebook, "drag-begin", G_CALLBACK (on_drag_begin), self);
  g_signal_connect (notebook, "drag-data-received", G_CALLBACK (on_drag_data_received), self);
  g_signal_connect (notebook, "drag-drop", G_CALLBACK (on_drag_drop), self);
  g_signal_connect (notebook, "drag-end", G_CALLBACK (on_drag_end), self);
  g_signal_connect (notebook, "drag-leave", G_CALLBACK (on_drag_leave), self);
  g_signal_connect (notebook, "drag-motion", G_CALLBACK (on_drag_motion), self);
  g_signal_connect (notebook, "switch-page", G_CALLBACK (on_switch_page), self);
  g_signal_connect (notebook, "set-focus-child", G_CALLBACK (on_set_focus_child), self);
  g_signal_connect (notebook, "create-window", G_CALLBACK (on_create_window), self);
  g_signal_connect (notebook, "remove", G_CALLBACK (on_remove), self);
}

void
gb_multi_notebook_insert_notebook (GbMultiNotebook *self,
                                   GbNotebook      *notebook,
                                   guint            position)
{
  GbMultiNotebookPrivate *priv;
  GtkWidget *notebook_container;
  GtkWidget *target_paned;
  GtkWidget *existing_child;

  g_return_if_fail (GB_IS_MULTI_NOTEBOOK (self));
  g_return_if_fail (GB_IS_NOTEBOOK (notebook));

  priv = self->priv;

  gtk_notebook_set_group_name (GTK_NOTEBOOK (notebook), priv->group_name);

  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), priv->show_tabs);

  position = CLAMP (position, 0, priv->notebooks->len);

  notebook_container = g_object_new (GTK_TYPE_PANED,
                                     "expand", TRUE,
                                     "visible", TRUE,
                                     NULL);
  gtk_paned_add1 (GTK_PANED (notebook_container), GTK_WIDGET (notebook));
  gtk_container_child_set (GTK_CONTAINER (notebook_container),
                           GTK_WIDGET (notebook),
                           "resize", TRUE,
                           "shrink", FALSE,
                           NULL);

  if (position == 0)
    target_paned = GTK_WIDGET (priv->root_paned);
  else
    {
      GtkWidget *widget;

      widget = g_ptr_array_index (priv->notebooks, position - 1);
      target_paned = gtk_widget_get_parent (widget);
    }

  g_assert (target_paned);
  g_assert (GTK_IS_PANED (target_paned));

  existing_child = gtk_paned_get_child2 (GTK_PANED (target_paned));

  g_assert (!existing_child || GTK_IS_PANED (existing_child));

  if (existing_child)
    {
      g_object_ref (existing_child);
      gtk_container_remove (GTK_CONTAINER (target_paned), existing_child);
      gtk_paned_add2 (GTK_PANED (notebook_container), existing_child);
      g_object_unref (existing_child);
    }

  gtk_paned_add2 (GTK_PANED (target_paned), notebook_container);

  g_ptr_array_insert (priv->notebooks, position, notebook);

  connect_notebook_signals (self, notebook);

  gb_multi_notebook_set_active_notebook (self, notebook);

  resize_paned_positions (self);
  force_redraw (self);

  g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs[PROP_N_NOTEBOOKS]);
}

static void
gb_multi_notebook_realize (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gb_multi_notebook_parent_class)->realize (widget);
  resize_paned_positions (GB_MULTI_NOTEBOOK (widget));
}

static void
gb_multi_notebook_dispose (GObject *object)
{
  GbMultiNotebookPrivate *priv = GB_MULTI_NOTEBOOK (object)->priv;

  if (priv->drag_drop_target)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->drag_drop_target),
                                    (gpointer *) &priv->drag_drop_target);
      priv->drag_drop_target = NULL;
    }

  g_clear_object (&priv->theatric);

  G_OBJECT_CLASS (gb_multi_notebook_parent_class)->dispose (object);
}

static void
gb_multi_notebook_finalize (GObject *object)
{
  GbMultiNotebookPrivate *priv;

  priv = GB_MULTI_NOTEBOOK (object)->priv;

  g_clear_pointer (&priv->notebooks, g_ptr_array_unref);

  G_OBJECT_CLASS (gb_multi_notebook_parent_class)->finalize (object);
}

static void
gb_multi_notebook_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GbMultiNotebook *notebook = GB_MULTI_NOTEBOOK (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_NOTEBOOK:
      g_value_set_object (value, gb_multi_notebook_get_active_notebook (notebook));
      break;

    case PROP_GROUP_NAME:
      g_value_set_string (value, gb_multi_notebook_get_group_name (notebook));
      break;

    case PROP_N_NOTEBOOKS:
      g_value_set_uint (value, gb_multi_notebook_get_n_notebooks (notebook));
      break;

    case PROP_SHOW_TABS:
      g_value_set_boolean (value, gb_multi_notebook_get_show_tabs (notebook));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_multi_notebook_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GbMultiNotebook *notebook = GB_MULTI_NOTEBOOK (object);

  switch (prop_id)
    {
    case PROP_GROUP_NAME:
      gb_multi_notebook_set_group_name (notebook, g_value_get_string (value));
      break;

    case PROP_SHOW_TABS:
      gb_multi_notebook_set_show_tabs (notebook, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_multi_notebook_class_init (GbMultiNotebookClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gb_multi_notebook_dispose;
  object_class->finalize = gb_multi_notebook_finalize;
  object_class->get_property = gb_multi_notebook_get_property;
  object_class->set_property = gb_multi_notebook_set_property;

  widget_class->realize = gb_multi_notebook_realize;

  gParamSpecs[PROP_ACTIVE_NOTEBOOK] =
    g_param_spec_object ("active-notebook",
                         _ ("Active Notebook"),
                         _ ("The active notebook for the multi-notebook."),
                         GB_TYPE_NOTEBOOK,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ACTIVE_NOTEBOOK,
                                   gParamSpecs[PROP_ACTIVE_NOTEBOOK]);

  gParamSpecs[PROP_ACTIVE_TAB] =
    g_param_spec_object ("active-tab",
                         _ ("Active Tab"),
                         _ ("The active tab."),
                         GB_TYPE_TAB,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ACTIVE_TAB,
                                   gParamSpecs[PROP_ACTIVE_TAB]);

  gParamSpecs[PROP_GROUP_NAME] =
    g_param_spec_string ("group-name",
                         _ ("Group Name"),
                         _ ("The group name for notebooks."),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_GROUP_NAME,
                                   gParamSpecs[PROP_GROUP_NAME]);

  gParamSpecs[PROP_N_NOTEBOOKS] =
    g_param_spec_uint ("n-notebooks",
                       _ ("Number of Notebooks"),
                       _ ("The number of notebooks in the widget"),
                       0,
                       G_MAXINT,
                       0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_N_NOTEBOOKS,
                                   gParamSpecs[PROP_N_NOTEBOOKS]);

  gParamSpecs[PROP_SHOW_TABS] =
    g_param_spec_boolean ("show-tabs",
                          _ ("Show Tabs"),
                          _ ("If we should show tabs for notebooks."),
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SHOW_TABS,
                                   gParamSpecs[PROP_SHOW_TABS]);

  gSignals[CREATE_WINDOW] =
    g_signal_new ("create-window",
                  GB_TYPE_MULTI_NOTEBOOK,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbMultiNotebookClass, create_window),
                  NULL,   /* gtk_object_handled_accumulator, */
                  NULL,
                  g_cclosure_marshal_generic,
                  GB_TYPE_NOTEBOOK,
                  4,
                  GB_TYPE_NOTEBOOK,
                  GTK_TYPE_WIDGET,
                  G_TYPE_INT,
                  G_TYPE_INT);
}

static void
gb_multi_notebook_init (GbMultiNotebook *self)
{
  GbMultiNotebookPrivate *priv;
  GtkWidget *notebook;

  priv = self->priv = gb_multi_notebook_get_instance_private (self);

  self->priv->show_tabs = TRUE;
  self->priv->notebooks = g_ptr_array_new ();

  priv->root_paned = g_object_new (GTK_TYPE_PANED,
                                   "expand", TRUE,
                                   "orientation", GTK_ORIENTATION_HORIZONTAL,
                                   "visible", TRUE,
                                   NULL);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (priv->root_paned));

  notebook = g_object_new (GB_TYPE_NOTEBOOK,
                           "visible", TRUE,
                           NULL);
  gb_multi_notebook_insert_notebook (self, GB_NOTEBOOK (notebook), 0);
}
