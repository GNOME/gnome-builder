/* ide-grid.c
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


#define G_LOG_DOMAIN "ide-grid"

#include "config.h"

#include <string.h>

#include "ide-grid.h"
#include "ide-gui-private.h"

/**
 * SECTION:ide-grid
 * @title: IdeGrid
 * @short_description: A grid for #IdePage
 *
 * The #IdeGrid provides a grid of pages that the user may
 * manipulate.
 *
 * Internally, this is implemented with #IdeGrid at the top
 * containing one or more of #IdeGridColumn. Those columns
 * contain one or more #IdeFrame. The stack can contain many
 * #IdePage.
 *
 * #IdeGrid implements the #GListModel interface to simplify
 * the process of listing (with deduplication) the pages that are
 * contianed within the #IdeGrid. If you would instead like
 * to see all possible pages in the stack, use the
 * ide_grid_foreach_page() API.
 *
 * Since: 3.32
 */

typedef struct
{
  /* Owned references */
  DzlSignalGroup *toplevel_signals;
  GQueue          focus_column;
  GArray         *stack_info;

  /*
   * This owned reference is our box highlight theatric that we
   * animate while doing a DnD drop interaction.
   */
  DzlBoxTheatric *drag_theatric;
  DzlAnimation   *drag_anim;

  /*
   * This unowned reference is simply used to compare to a new focus
   * page to see if we have changed our current page. It is not to
   * be used directly, only for pointer comparison.
   */
  IdePage  *_last_focused_page;

  /*
   * A GSource that is used to remove empty stacks that are unnecessary
   * (after a last stack item is removed).
   */
  guint cull_source;
} IdeGridPrivate;

typedef struct
{
  IdeGridColumn *column;
  IdeFrame      *stack;
  GdkRectangle         area;
  gint                 drop;
  gint                 x;
  gint                 y;
} DropLocate;

typedef struct
{
  IdeFrame *stack;
  guint           len;
} StackInfo;

enum {
  PROP_0,
  PROP_CURRENT_COLUMN,
  PROP_CURRENT_STACK,
  PROP_CURRENT_PAGE,
  N_PROPS
};

enum {
  CREATE_FRAME,
  CREATE_VIEW,
  N_SIGNALS
};

enum {
  DROP_ONTO,
  DROP_ABOVE,
  DROP_BELOW,
  DROP_LEFT_OF,
  DROP_RIGHT_OF,
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeGrid, ide_grid, DZL_TYPE_MULTI_PANED,
                         G_ADD_PRIVATE (IdeGrid)
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
ide_grid_cull (IdeGrid *self)
{
  guint n_columns;

  g_assert (IDE_IS_GRID (self));

  n_columns = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self));

  for (guint i = n_columns; i > 0; i--)
    {
      IdeGridColumn *column;
      guint n_stacks;

      column = IDE_GRID_COLUMN (dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), i - 1));
      n_stacks = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (column));

      if (n_columns == 1 && n_stacks == 1)
        return;

      for (guint j = n_stacks; j > 0; j--)
        {
          IdeFrame *stack;
          guint n_items;

          stack = IDE_FRAME (dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (column), j - 1));
          n_items = g_list_model_get_n_items (G_LIST_MODEL (stack));

          if (n_items == 0)
            gtk_widget_destroy (GTK_WIDGET (stack));
        }

      if (dzl_multi_paned_get_n_children (DZL_MULTI_PANED (column)) == 0)
        gtk_widget_destroy (GTK_WIDGET (column));
    }
}

static gboolean
ide_grid_do_cull (gpointer data)
{
  IdeGrid *self = data;
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);

  g_assert (IDE_IS_GRID (self));

  priv->cull_source = 0;

  ide_grid_cull (self);

  return G_SOURCE_REMOVE;
}

static void
ide_grid_queue_cull (IdeGrid *self)
{
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);

  g_assert (IDE_IS_GRID (self));

  if (priv->cull_source != 0)
    return;

  priv->cull_source = gdk_threads_add_idle_full (G_PRIORITY_HIGH,
                                                 ide_grid_do_cull,
                                                 g_object_ref (self),
                                                 g_object_unref);
}

static void
ide_grid_update_actions (IdeGrid *self)
{
  guint n_children;

  g_assert (IDE_IS_GRID (self));

  n_children = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self));

  for (guint i = 0; i < n_children; i++)
    {
      GtkWidget *column = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), i);

      g_assert (IDE_IS_GRID_COLUMN (column));

      _ide_grid_column_update_actions (IDE_GRID_COLUMN (column));
    }
}

static IdeFrame *
ide_grid_real_create_frame (IdeGrid *self)
{
  return g_object_new (IDE_TYPE_FRAME,
                       "expand", TRUE,
                       "visible", TRUE,
                       NULL);
}

static GtkWidget *
ide_grid_create_frame (IdeGrid *self)
{
  IdeFrame *ret = NULL;

  g_assert (IDE_IS_GRID (self));

  g_signal_emit (self, signals [CREATE_FRAME], 0, &ret);
  g_return_val_if_fail (IDE_IS_FRAME (ret), NULL);
  return GTK_WIDGET (ret);
}

static GtkWidget *
ide_grid_create_column (IdeGrid *self)
{
  GtkWidget *stack;

  g_assert (IDE_IS_GRID (self));

  stack = ide_grid_create_frame (self);

  if (stack != NULL)
    {
      GtkWidget *column = g_object_new (IDE_TYPE_GRID_COLUMN,
                                        "visible", TRUE,
                                        NULL);
      gtk_container_add (GTK_CONTAINER (column), stack);
      return column;
    }

  return NULL;
}

static void
ide_grid_after_set_focus (IdeGrid *self,
                                 GtkWidget     *widget,
                                 GtkWidget     *toplevel)
{
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);

  g_assert (IDE_IS_GRID (self));
  g_assert (!widget || GTK_IS_WIDGET (widget));
  g_assert (GTK_IS_WINDOW (toplevel));

  if (widget != NULL)
    {
      GtkWidget *column = NULL;
      GtkWidget *page;

      if (gtk_widget_is_ancestor (widget, GTK_WIDGET (self)))
        {
          column = gtk_widget_get_ancestor (widget, IDE_TYPE_GRID_COLUMN);

          if (column != NULL)
            ide_grid_set_current_column (self, IDE_GRID_COLUMN (column));
        }

      /*
       * self->_last_focused_page is an unowned reference, we only
       * use it for pointer comparison, nothing more.
       */
      page = gtk_widget_get_ancestor (widget, IDE_TYPE_PAGE);
      if (page != (GtkWidget *)priv->_last_focused_page)
        {
          priv->_last_focused_page = (IdePage *)page;
          ide_object_notify_in_main (self, properties [PROP_CURRENT_PAGE]);

          if (page != NULL && column != NULL)
            {
              GtkWidget *stack;

              stack = gtk_widget_get_ancestor (GTK_WIDGET (page), IDE_TYPE_FRAME);
              if (stack != NULL)
                ide_grid_column_set_current_stack (IDE_GRID_COLUMN (column),
                                                          IDE_FRAME (stack));
            }
        }
    }
}

static void
ide_grid_hierarchy_changed (GtkWidget *widget,
                                   GtkWidget *old_toplevel)
{
  IdeGrid *self = (IdeGrid *)widget;
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);
  GtkWidget *toplevel;

  g_assert (IDE_IS_GRID (self));
  g_assert (!old_toplevel || GTK_IS_WIDGET (old_toplevel));

  /*
   * Setup focus tracking so that we can update our "current stack" when the
   * user selected focus changes.
   */

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    dzl_signal_group_set_target (priv->toplevel_signals, toplevel);
  else
    dzl_signal_group_set_target (priv->toplevel_signals, NULL);

  /*
   * If we've been added to a widget and still do not have a stack added, then
   * we'll emit our ::create-frame signal to create that now. We do this here
   * to allow the consumer to connect to ::create-frame before adding the
   * widget to the hierarchy.
   */

  if (dzl_multi_paned_get_n_children (DZL_MULTI_PANED (widget)) == 0)
    {
      GtkWidget *column = ide_grid_create_column (self);
      gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (column));
    }
}

static void
ide_grid_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  IdeGrid *self = (IdeGrid *)container;
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);

  g_assert (IDE_IS_GRID (self));
  g_assert (GTK_IS_WIDGET (widget));

  if (IDE_IS_GRID_COLUMN (widget))
    {
      GList *children;

      /* Add our column to the grid */
      g_queue_push_head (&priv->focus_column, widget);
      GTK_CONTAINER_CLASS (ide_grid_parent_class)->add (container, widget);
      ide_grid_set_current_column (self, IDE_GRID_COLUMN (widget));
      _ide_grid_column_update_actions (IDE_GRID_COLUMN (widget));

      /* Start monitoring all the stacks in the grid for pages */
      children = gtk_container_get_children (GTK_CONTAINER (widget));
      for (const GList *iter = children; iter; iter = iter->next)
        if (IDE_IS_FRAME (iter->data))
          _ide_grid_stack_added (self, iter->data);
      g_list_free (children);
    }
  else if (IDE_IS_FRAME (widget))
    {
      IdeGridColumn *column;

      column = ide_grid_get_current_column (self);
      gtk_container_add (GTK_CONTAINER (column), widget);
      ide_grid_set_current_column (self, column);
    }
  else if (IDE_IS_PAGE (widget))
    {
      IdeGridColumn *column = NULL;
      guint n_columns;

      /* If we have an empty layout stack, we'll prefer to add the
       * page to that. If we don't find an empty stack, we'll add
       * the page to the most recently focused stack.
       */

      n_columns = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self));

      for (guint i = 0; i < n_columns; i++)
        {
          GtkWidget *ele = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), i);

          g_assert (IDE_IS_GRID_COLUMN (ele));

          if (_ide_grid_column_is_empty (IDE_GRID_COLUMN (ele)))
            {
              column = IDE_GRID_COLUMN (ele);
              break;
            }
        }

      if (column == NULL)
        column = ide_grid_get_current_column (self);

      g_assert (IDE_IS_GRID_COLUMN (column));

      gtk_container_add (GTK_CONTAINER (column), widget);
    }
  else
    {
      g_warning ("%s must be one of IdeFrame, IdePage, or IdeGrid",
                 G_OBJECT_TYPE_NAME (self));
      return;
    }

  ide_grid_update_actions (self);
}

static void
ide_grid_remove (GtkContainer *container,
                        GtkWidget    *widget)
{
  IdeGrid *self = (IdeGrid *)container;
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);
  gboolean notify = FALSE;

  g_assert (IDE_IS_GRID (self));
  g_assert (IDE_IS_GRID_COLUMN (widget));

  notify = g_queue_peek_head (&priv->focus_column) == (gpointer)widget;
  g_queue_remove (&priv->focus_column, widget);

  GTK_CONTAINER_CLASS (ide_grid_parent_class)->remove (container, widget);

  ide_grid_update_actions (self);

  if (notify)
    {
      GtkWidget *head = g_queue_peek_head (&priv->focus_column);

      if (head != NULL)
        gtk_widget_grab_focus (head);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CURRENT_COLUMN]);
    }
}

static gboolean
ide_grid_get_drop_area (IdeGrid        *self,
                               gint                  x,
                               gint                  y,
                               GdkRectangle         *out_area,
                               IdeGridColumn **out_column,
                               IdeFrame      **out_stack,
                               gint                 *out_drop)
{
  GtkAllocation alloc;
  GtkWidget *column;
  GtkWidget *stack = NULL;

  g_assert (IDE_IS_GRID (self));
  g_assert (out_area != NULL);
  g_assert (out_column != NULL);
  g_assert (out_stack != NULL);
  g_assert (out_drop != NULL);

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  column = dzl_multi_paned_get_at_point (DZL_MULTI_PANED (self), x + alloc.x, 0);
  if (column != NULL)
    stack = dzl_multi_paned_get_at_point (DZL_MULTI_PANED (column), 0, y + alloc.y);

  if (column != NULL && stack != NULL)
    {
      GtkAllocation stack_alloc;

      gtk_widget_get_allocation (stack, &stack_alloc);

      gtk_widget_translate_coordinates (stack,
                                        GTK_WIDGET (self),
                                        0, 0,
                                        &stack_alloc.x, &stack_alloc.y);

      *out_area = stack_alloc;
      *out_column = IDE_GRID_COLUMN (column);
      *out_stack = IDE_FRAME (stack);
      *out_drop = DROP_ONTO;

      gtk_widget_translate_coordinates (GTK_WIDGET (self), stack, x, y, &x, &y);

      if (FALSE) {}
      else if (x < (stack_alloc.width / 4))
        {
          out_area->y = 0;
          out_area->height = alloc.height;
          out_area->width = stack_alloc.width / 4;
          *out_drop = DROP_LEFT_OF;
        }
      else if (x > (stack_alloc.width / 4 * 3))
        {
          out_area->y = 0;
          out_area->height = alloc.height;
          out_area->x = dzl_cairo_rectangle_x2 (&stack_alloc) - (stack_alloc.width / 4);
          out_area->width = stack_alloc.width / 4;
          *out_drop = DROP_RIGHT_OF;
        }
      else if (y < (stack_alloc.height / 4))
        {
          out_area->height = stack_alloc.height / 4;
          *out_drop = DROP_ABOVE;
        }
      else if (y > (stack_alloc.height / 4 * 3))
        {
          out_area->y = dzl_cairo_rectangle_y2 (&stack_alloc) - (stack_alloc.height / 4);
          out_area->height = stack_alloc.height / 4;
          *out_drop = DROP_BELOW;
        }

      return TRUE;
    }

  return FALSE;
}

static gboolean
ide_grid_drag_motion (GtkWidget      *widget,
                             GdkDragContext *context,
                             gint            x,
                             gint            y,
                             guint           time_)
{
  IdeGrid *self = (IdeGrid *)widget;
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);
  IdeGridColumn *column = NULL;
  IdeFrame *stack = NULL;
  DzlAnimation *drag_anim;
  GdkRectangle area = {0};
  GtkAllocation alloc;
  gint drop = DROP_ONTO;

  g_assert (IDE_IS_GRID (self));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  if (priv->drag_anim != NULL)
    {
      dzl_animation_stop (priv->drag_anim);
      g_clear_weak_pointer (&priv->drag_anim);
    }

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  if (!ide_grid_get_drop_area (self, x, y, &area, &column, &stack, &drop))
    return GDK_EVENT_PROPAGATE;

  if (priv->drag_theatric == NULL)
    {
      priv->drag_theatric = g_object_new (DZL_TYPE_BOX_THEATRIC,
                                          "x", area.x,
                                          "y", area.y,
                                          "width", area.width,
                                          "height", area.height,
                                          "alpha", 0.3,
                                          "background", "#729fcf",
                                          "target", self,
                                          NULL);
      return GDK_EVENT_STOP;
    }

  drag_anim = dzl_object_animate (priv->drag_theatric,
                                  DZL_ANIMATION_EASE_OUT_CUBIC,
                                  100,
                                  gtk_widget_get_frame_clock (GTK_WIDGET (self)),
                                  "x", area.x,
                                  "width", area.width,
                                  "y", area.y,
                                  "height", area.height,
                                  NULL);
  g_set_weak_pointer (&priv->drag_anim, drag_anim);

  gtk_widget_queue_draw (GTK_WIDGET (self));

  return GDK_EVENT_STOP;
}

static void
ide_grid_drag_data_received (GtkWidget        *widget,
                                    GdkDragContext   *context,
                                    gint              x,
                                    gint              y,
                                    GtkSelectionData *data,
                                    guint             info,
                                    guint             time_)
{
  IdeGrid *self = (IdeGrid *)widget;
  IdeGridColumn *column = NULL;
  IdeFrame *stack = NULL;
  g_auto(GStrv) uris = NULL;
  GdkRectangle area = {0};
  gint drop = DROP_ONTO;

  g_assert (IDE_IS_GRID (self));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  if (!ide_grid_get_drop_area (self, x, y, &area, &column, &stack, &drop))
    return;

  g_assert (IDE_IS_GRID_COLUMN (column));
  g_assert (IDE_IS_FRAME (stack));

  if (!(uris = gtk_selection_data_get_uris (data)))
    return;

  for (guint i = 0; uris[i] != NULL; i++)
    {
      const gchar *uri = uris[i];
      IdePage *page = NULL;
      gint column_index = 0;
      gint stack_index = 0;

      g_signal_emit (self, signals [CREATE_VIEW], 0, uri, &page);

      if (page == NULL)
        {
          g_debug ("Failed to load IdePage for \"%s\"", uri);
          continue;
        }

      gtk_container_child_get (GTK_CONTAINER (self), GTK_WIDGET (column),
                               "index", &column_index,
                               NULL);
      gtk_container_child_get (GTK_CONTAINER (column), GTK_WIDGET (stack),
                               "index", &stack_index,
                               NULL);

      switch (drop)
        {
        case DROP_ONTO:
          gtk_container_add (GTK_CONTAINER (stack), GTK_WIDGET (page));
          break;

        case DROP_ABOVE:
          stack = IDE_FRAME (ide_grid_create_frame (self));
          gtk_container_add_with_properties (GTK_CONTAINER (column), GTK_WIDGET (stack),
                                             "index", stack_index,
                                             NULL);
          gtk_container_add (GTK_CONTAINER (stack), GTK_WIDGET (page));
          break;

        case DROP_BELOW:
          stack = IDE_FRAME (ide_grid_create_frame (self));
          gtk_container_add_with_properties (GTK_CONTAINER (column), GTK_WIDGET (stack),
                                             "index", stack_index + 1,
                                             NULL);
          gtk_container_add (GTK_CONTAINER (stack), GTK_WIDGET (page));
          break;

        case DROP_LEFT_OF:
          column = IDE_GRID_COLUMN (ide_grid_create_column (self));
          gtk_container_add_with_properties (GTK_CONTAINER (self), GTK_WIDGET (column),
                                             "index", column_index,
                                             NULL);
          gtk_container_add (GTK_CONTAINER (column), GTK_WIDGET (page));
          break;

        case DROP_RIGHT_OF:
          column = IDE_GRID_COLUMN (ide_grid_create_column (self));
          gtk_container_add_with_properties (GTK_CONTAINER (self), GTK_WIDGET (column),
                                             "index", column_index + 1,
                                             NULL);
          gtk_container_add (GTK_CONTAINER (column), GTK_WIDGET (page));
          break;

        default:
          g_assert_not_reached ();
        }
    }
}

static void
ide_grid_drag_leave (GtkWidget      *widget,
                            GdkDragContext *context,
                            guint           time_)
{
  IdeGrid *self = (IdeGrid *)widget;
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);

  g_assert (IDE_IS_GRID (self));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  if (priv->drag_anim != NULL)
    {
      dzl_animation_stop (priv->drag_anim);
      g_clear_weak_pointer (&priv->drag_anim);
    }

  g_clear_object (&priv->drag_theatric);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static gboolean
ide_grid_drag_failed (GtkWidget      *widget,
                             GdkDragContext *context,
                             GtkDragResult   result)
{
  IdeGrid *self = (IdeGrid *)widget;
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);

  g_assert (IDE_IS_GRID (self));
  g_assert (GDK_IS_DRAG_CONTEXT (context));

  if (priv->drag_anim != NULL)
    {
      dzl_animation_stop (priv->drag_anim);
      g_clear_weak_pointer (&priv->drag_anim);
    }

  g_clear_object (&priv->drag_theatric);
  gtk_widget_queue_draw (GTK_WIDGET (self));

  return GDK_EVENT_PROPAGATE;
}

static void
ide_grid_grab_focus (GtkWidget *widget)
{
  IdeGrid *self = (IdeGrid *)widget;
  IdeFrame *stack;

  g_assert (IDE_IS_GRID (self));

  stack = ide_grid_get_current_stack (self);

  if (stack != NULL)
    gtk_widget_grab_focus (GTK_WIDGET (stack));
  else
    GTK_WIDGET_CLASS (ide_grid_parent_class)->grab_focus (widget);
}

static void
ide_grid_destroy (GtkWidget *widget)
{
  IdeGrid *self = (IdeGrid *)widget;
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);

  dzl_clear_source (&priv->cull_source);

  GTK_WIDGET_CLASS (ide_grid_parent_class)->destroy (widget);
}

static void
ide_grid_finalize (GObject *object)
{
  IdeGrid *self = (IdeGrid *)object;
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);

  g_assert (IDE_IS_GRID (self));
  g_assert (priv->focus_column.head == NULL);
  g_assert (priv->focus_column.tail == NULL);
  g_assert (priv->focus_column.length == 0);

  g_clear_pointer (&priv->stack_info, g_array_unref);
  g_clear_object (&priv->toplevel_signals);

  G_OBJECT_CLASS (ide_grid_parent_class)->finalize (object);
}

static void
ide_grid_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  IdeGrid *self = IDE_GRID (object);

  switch (prop_id)
    {
    case PROP_CURRENT_COLUMN:
      g_value_set_object (value, ide_grid_get_current_column (self));
      break;

    case PROP_CURRENT_STACK:
      g_value_set_object (value, ide_grid_get_current_stack (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_grid_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  IdeGrid *self = IDE_GRID (object);

  switch (prop_id)
    {
    case PROP_CURRENT_COLUMN:
      ide_grid_set_current_column (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_grid_class_init (IdeGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = ide_grid_finalize;
  object_class->get_property = ide_grid_get_property;
  object_class->set_property = ide_grid_set_property;

  widget_class->destroy = ide_grid_destroy;
  widget_class->drag_data_received = ide_grid_drag_data_received;
  widget_class->drag_motion = ide_grid_drag_motion;
  widget_class->drag_leave = ide_grid_drag_leave;
  widget_class->drag_failed = ide_grid_drag_failed;
  widget_class->grab_focus = ide_grid_grab_focus;
  widget_class->hierarchy_changed = ide_grid_hierarchy_changed;

  container_class->add = ide_grid_add;
  container_class->remove = ide_grid_remove;

  klass->create_frame = ide_grid_real_create_frame;

  properties [PROP_CURRENT_COLUMN] =
    g_param_spec_object ("current-column",
                         "Current Column",
                         "The most recently focused grid column",
                         IDE_TYPE_GRID_COLUMN,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CURRENT_STACK] =
    g_param_spec_object ("current-stack",
                         "Current Stack",
                         "The most recently focused IdeFrame",
                         IDE_TYPE_FRAME,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CURRENT_PAGE] =
    g_param_spec_object ("current-page",
                         "Current View",
                         "The most recently focused IdePage",
                         IDE_TYPE_PAGE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "idegrid");

  /**
   * IdeGrid::create-frame:
   * @self: an #IdeGrid
   *
   * Creates a new stack to be added to the grid.
   *
   * Returns: (transfer full): A newly created #IdeFrame
   *
   * Since: 3.34
   */
  signals [CREATE_FRAME] =
    g_signal_new (g_intern_static_string ("create-frame"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeGridClass, create_frame),
                  g_signal_accumulator_first_wins, NULL, NULL,
                  IDE_TYPE_FRAME, 0);

  /**
   * IdeGrid::create-page:
   * @self: an #IdeGrid
   * @uri: the URI to open
   *
   * Creates a new page for @uri to be added to the grid.
   *
   * Returns: (transfer full): A newly created #IdePage
   *
   * Since: 3.32
   */
  signals [CREATE_VIEW] =
    g_signal_new (g_intern_static_string ("create-page"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeGridClass, create_page),
                  g_signal_accumulator_first_wins, NULL, NULL,
                  IDE_TYPE_PAGE,
                  1,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
ide_grid_init (IdeGrid *self)
{
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);
  static const GtkTargetEntry target_entries[] = {
    { (gchar *)"text/uri-list", 0, 0 },
  };

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_HORIZONTAL);

  gtk_drag_dest_set (GTK_WIDGET (self),
                     GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                     target_entries,
                     G_N_ELEMENTS (target_entries),
                     GDK_ACTION_COPY);

  priv->stack_info = g_array_new (FALSE, FALSE, sizeof (StackInfo));

  priv->toplevel_signals = dzl_signal_group_new (GTK_TYPE_WINDOW);

  dzl_signal_group_connect_object (priv->toplevel_signals,
                                   "set-focus",
                                   G_CALLBACK (ide_grid_after_set_focus),
                                   self,
                                   G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  _ide_grid_init_actions (self);
}

/**
 * ide_grid_new:
 *
 * Creates a new #IdeGrid.
 *
 * Returns: (transfer full): A newly created #IdeGrid
 *
 * Since: 3.32
 */
GtkWidget *
ide_grid_new (void)
{
  return g_object_new (IDE_TYPE_GRID, NULL);
}

/**
 * ide_grid_get_current_stack:
 * @self: a #IdeGrid
 *
 * Gets the most recently focused stack. This is useful when you want to open
 * a document on the stack the user last focused.
 *
 * Returns: (transfer none) (nullable): an #IdeFrame or %NULL.
 *
 * Since: 3.32
 */
IdeFrame *
ide_grid_get_current_stack (IdeGrid *self)
{
  IdeGridColumn *column;

  g_return_val_if_fail (IDE_IS_GRID (self), NULL);

  column = ide_grid_get_current_column (self);
  if (column != NULL)
    return ide_grid_column_get_current_stack (column);

  return NULL;
}

/**
 * ide_grid_get_nth_column:
 * @self: a #IdeGrid
 * @nth: the index of the column, or -1
 *
 * Gets the @nth column from the grid.
 *
 * If @nth is -1, then a new column at the beginning of the
 * grid is created.
 *
 * If @nth is >= the number of columns in the grid, then a new
 * column at the end of the grid is created.
 *
 * Returns: (transfer none): An #IdeGridColumn.
 *
 * Since: 3.32
 */
IdeGridColumn *
ide_grid_get_nth_column (IdeGrid *self,
                                gint           nth)
{
  GtkWidget *column;

  g_return_val_if_fail (IDE_IS_GRID (self), NULL);

  if (nth < 0)
    {
      column = ide_grid_create_column (self);
      gtk_container_add_with_properties (GTK_CONTAINER (self), column,
                                         "index", 0,
                                         NULL);
    }
  else if (nth >= dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self)))
    {
      column = ide_grid_create_column (self);
      gtk_container_add (GTK_CONTAINER (self), column);
    }
  else
    {
      column = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), nth);
    }

  g_return_val_if_fail (IDE_IS_GRID_COLUMN (column), NULL);

  return IDE_GRID_COLUMN (column);
}

/*
 * _ide_grid_get_nth_stack:
 *
 * This will get the @nth stack. If it does not yet exist,
 * it will be created.
 *
 * If nth == -1, a new stack will be created at index 0.
 *
 * If nth >= the number of stacks, a new stack will be created
 * at the end of the grid.
 *
 * Returns: (not nullable) (transfer none): An #IdeFrame.
 */
IdeFrame *
_ide_grid_get_nth_stack (IdeGrid *self,
                                gint           nth)
{
  IdeGridColumn *column;
  IdeFrame *stack;

  g_return_val_if_fail (IDE_IS_GRID (self), NULL);

  column = ide_grid_get_nth_column (self, nth);
  stack = ide_grid_column_get_current_stack (IDE_GRID_COLUMN (column));

  g_return_val_if_fail (IDE_IS_FRAME (stack), NULL);

  return stack;
}

/**
 * _ide_grid_get_nth_stack_for_column:
 * @self: an #IdeGrid
 * @column: an #IdeGridColumn
 * @nth: the index of the column, between -1 and G_MAXINT
 *
 * This will get the @nth stack within @column. If a matching stack
 * cannot be found, it will be created.
 *
 * If @nth is less-than 0, a new column will be inserted at the top.
 *
 * If @nth is greater-than the number of stacks, then a new stack
 * will be created at the bottom.
 *
 * Returns: (not nullable) (transfer none): An #IdeFrame.
 *
 * Since: 3.32
 */
IdeFrame *
_ide_grid_get_nth_stack_for_column (IdeGrid       *self,
                                           IdeGridColumn *column,
                                           gint                 nth)
{
  GtkWidget *stack;

  g_return_val_if_fail (IDE_IS_GRID (self), NULL);
  g_return_val_if_fail (IDE_IS_GRID_COLUMN (column), NULL);
  g_return_val_if_fail (gtk_widget_get_parent (GTK_WIDGET (column)) == GTK_WIDGET (self), NULL);

  if (nth < 0)
    {
      stack = ide_grid_create_frame (self);
      gtk_container_add_with_properties (GTK_CONTAINER (column), stack,
                                         "index", 0,
                                         NULL);
    }
  else if (nth >= dzl_multi_paned_get_n_children (DZL_MULTI_PANED (column)))
    {
      stack = ide_grid_create_frame (self);
      gtk_container_add (GTK_CONTAINER (self), stack);
    }
  else
    {
      stack = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (column), nth);
    }

  g_assert (IDE_IS_FRAME (stack));

  return IDE_FRAME (stack);
}

/**
 * ide_grid_get_current_column:
 * @self: a #IdeGrid
 *
 * Gets the most recently focused column of the grid.
 *
 * Returns: (transfer none) (not nullable): An #IdeGridColumn
 *
 * Since: 3.32
 */
IdeGridColumn *
ide_grid_get_current_column (IdeGrid *self)
{
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);
  GtkWidget *ret = NULL;

  g_return_val_if_fail (IDE_IS_GRID (self), NULL);

  if (priv->focus_column.head != NULL)
    ret = priv->focus_column.head->data;
  else if (dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self)) > 0)
    ret = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), 0);

  if (ret == NULL)
    {
      ret = ide_grid_create_column (self);
      gtk_container_add (GTK_CONTAINER (self), ret);
    }

  g_return_val_if_fail (IDE_IS_GRID_COLUMN (ret), NULL);

  return IDE_GRID_COLUMN (ret);
}

/**
 * ide_grid_set_current_column:
 * @self: an #IdeGrid
 * @column: (nullable): an #IdeGridColumn or %NULL
 *
 * Sets the current column for the grid. Generally this is automatically
 * updated for you when the focus changes within the workbench.
 *
 * @column can be %NULL out of convenience.
 *
 * Since: 3.32
 */
void
ide_grid_set_current_column (IdeGrid       *self,
                                    IdeGridColumn *column)
{
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);
  GList *iter;

  g_return_if_fail (IDE_IS_GRID (self));
  g_return_if_fail (!column || IDE_IS_GRID_COLUMN (column));

  if (column == NULL)
    return;

  if (gtk_widget_get_parent (GTK_WIDGET (column)) != GTK_WIDGET (self))
    {
      g_warning ("Attempt to set current column with non-descendant");
      return;
    }

  if (NULL != (iter = g_queue_find (&priv->focus_column, column)))
    {
      g_queue_unlink (&priv->focus_column, iter);
      g_queue_push_head_link (&priv->focus_column, iter);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CURRENT_COLUMN]);
      ide_grid_update_actions (self);
      return;
    }

  g_warning ("%s does not contain %s",
             G_OBJECT_TYPE_NAME (self), G_OBJECT_TYPE_NAME (column));
}

/**
 * ide_grid_get_current_page:
 * @self: a #IdeGrid
 *
 * Gets the most recent page used by the user as determined by tracking
 * the window focus.
 *
 * Returns: (transfer none): An #IdePage or %NULL
 *
 * Since: 3.32
 */
IdePage *
ide_grid_get_current_page (IdeGrid *self)
{
  IdeFrame *stack;

  g_return_val_if_fail (IDE_IS_GRID (self), NULL);

  stack = ide_grid_get_current_stack (self);

  if (stack != NULL)
    return ide_frame_get_visible_child (stack);

  return NULL;
}

static void
collect_pages (GtkWidget *widget,
               GPtrArray *ar)
{
  if (IDE_IS_PAGE (widget))
    g_ptr_array_add (ar, widget);
}

/**
 * ide_grid_foreach_page:
 * @self: a #IdeGrid
 * @callback: (scope call) (closure user_data): A callback for each page
 * @user_data: user data for @callback
 *
 * This function will call @callback for every page found in @self.
 *
 * Since: 3.32
 */
void
ide_grid_foreach_page (IdeGrid     *self,
                       GtkCallback  callback,
                       gpointer     user_data)
{
  g_autoptr(GPtrArray) pages = NULL;
  guint n_columns;

  g_return_if_fail (IDE_IS_GRID (self));
  g_return_if_fail (callback != NULL);

  pages = g_ptr_array_new ();

  n_columns = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self));

  for (guint i = 0; i < n_columns; i++)
    {
      GtkWidget *column = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), i);
      guint n_stacks;

      g_assert (IDE_IS_GRID_COLUMN (column));

      n_stacks = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (column));

      for (guint j = 0; j < n_stacks; j++)
        {
          GtkWidget *stack = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (column), j);

          g_assert (IDE_IS_FRAME (stack));

          ide_frame_foreach_page (IDE_FRAME (stack),
                                  (GtkCallback) collect_pages,
                                  pages);
        }
    }

  for (guint i = 0; i < pages->len; i++)
    callback (g_ptr_array_index (pages, i), user_data);
}

static GType
ide_grid_get_item_type (GListModel *model)
{
  return IDE_TYPE_PAGE;
}

static guint
ide_grid_get_n_items (GListModel *model)
{
  IdeGrid *self = (IdeGrid *)model;
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);
  guint n_items = 0;

  g_assert (IDE_IS_GRID (self));

  for (guint i = 0; i < priv->stack_info->len; i++)
    n_items += g_array_index (priv->stack_info, StackInfo, i).len;

  return n_items;
}

static gpointer
ide_grid_get_item (GListModel *model,
                          guint       position)
{
  IdeGrid *self = (IdeGrid *)model;
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);

  g_assert (IDE_IS_GRID (self));
  g_assert (position < ide_grid_get_n_items (model));

  for (guint i = 0; i < priv->stack_info->len; i++)
    {
      const StackInfo *info = &g_array_index (priv->stack_info, StackInfo, i);

      if (position >= info->len)
        {
          position -= info->len;
          continue;
        }

      return g_list_model_get_item (G_LIST_MODEL (info->stack), position);
    }

  g_warning ("Failed to locate position %u within %s",
             position, G_OBJECT_TYPE_NAME (self));

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_grid_get_item_type;
  iface->get_n_items = ide_grid_get_n_items;
  iface->get_item = ide_grid_get_item;
}

static void
ide_grid_stack_items_changed (IdeGrid  *self,
                                     guint           position,
                                     guint           removed,
                                     guint           added,
                                     IdeFrame *stack)
{
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);
  guint real_position = 0;

  g_assert (IDE_IS_GRID (self));
  g_assert (IDE_IS_FRAME (stack));

  for (guint i = 0; i < priv->stack_info->len; i++)
    {
      StackInfo *info = &g_array_index (priv->stack_info, StackInfo, i);

      if (info->stack == stack)
        {
          info->len -= removed;
          info->len += added;

          g_list_model_items_changed (G_LIST_MODEL (self),
                                      real_position + position,
                                      removed,
                                      added);

          ide_object_notify_in_main (G_OBJECT (self), properties [PROP_CURRENT_PAGE]);

          ide_grid_queue_cull (self);

          return;
        }

      real_position += info->len;
    }

  g_warning ("Failed to locate %s within %s",
             G_OBJECT_TYPE_NAME (stack), G_OBJECT_TYPE_NAME (self));
}

void
_ide_grid_stack_added (IdeGrid  *self,
                              IdeFrame *stack)
{
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);
  StackInfo info = { 0 };
  guint n_items;

  g_return_if_fail (IDE_IS_GRID (self));
  g_return_if_fail (IDE_IS_FRAME (stack));
  g_return_if_fail (G_IS_LIST_MODEL (stack));

  info.stack = stack;
  info.len = 0;

  g_array_append_val (priv->stack_info, info);

  g_signal_connect_object (stack,
                           "items-changed",
                           G_CALLBACK (ide_grid_stack_items_changed),
                           self,
                           G_CONNECT_SWAPPED);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (stack));
  ide_grid_stack_items_changed (self, 0, 0, n_items, stack);
}

void
_ide_grid_stack_removed (IdeGrid  *self,
                                IdeFrame *stack)
{
  IdeGridPrivate *priv = ide_grid_get_instance_private (self);
  guint position = 0;

  g_return_if_fail (IDE_IS_GRID (self));
  g_return_if_fail (IDE_IS_FRAME (stack));

  g_signal_handlers_disconnect_by_func (stack,
                                        G_CALLBACK (ide_grid_stack_items_changed),
                                        self);

  for (guint i = 0; i < priv->stack_info->len; i++)
    {
      const StackInfo info = g_array_index (priv->stack_info, StackInfo, i);

      if (info.stack == stack)
        {
          g_array_remove_index (priv->stack_info, i);
          g_list_model_items_changed (G_LIST_MODEL (self), position, info.len, 0);
          break;
        }
    }
}

static void
count_pages_cb (GtkWidget *widget,
                gpointer   data)
{
  (*(guint *)data)++;
}

guint
ide_grid_count_pages (IdeGrid *self)
{
  guint count = 0;

  g_return_val_if_fail (IDE_IS_GRID (self), 0);

  ide_grid_foreach_page (self, count_pages_cb, &count);

  return count;
}

/**
 * ide_grid_focus_neighbor:
 * @self: An #IdeGrid
 * @dir: the direction for the focus change
 *
 * Attempts to focus a neighbor #IdePage in the grid based on
 * the direction requested.
 *
 * If an #IdePage was focused, it will be returned to the caller.
 *
 * Returns: (transfer none) (nullable): An #IdePage or %NULL
 *
 * Since: 3.32
 */
IdePage *
ide_grid_focus_neighbor (IdeGrid    *self,
                                GtkDirectionType  dir)
{
  IdeGridColumn *column;
  IdeFrame *stack;
  IdePage *page = NULL;
  guint stack_pos = 0;
  guint column_pos = 0;
  guint n_children;

  g_return_val_if_fail (IDE_IS_GRID (self), NULL);
  g_return_val_if_fail (dir <= GTK_DIR_RIGHT, NULL);

  /* Make sure we have a current page and stack */
  if (NULL == (stack = ide_grid_get_current_stack (self)) ||
      NULL == (column = ide_grid_get_current_column (self)))
    return NULL;

  gtk_container_child_get (GTK_CONTAINER (self), GTK_WIDGET (column),
                           "index", &column_pos,
                           NULL);

  gtk_container_child_get (GTK_CONTAINER (column), GTK_WIDGET (stack),
                           "index", &stack_pos,
                           NULL);

  switch (dir)
    {
    case GTK_DIR_DOWN:
      n_children = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (column));
      if (n_children - stack_pos == 1)
        return NULL;
      stack = IDE_FRAME (dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (column), stack_pos + 1));
      page = ide_frame_get_visible_child (stack);
      break;

    case GTK_DIR_RIGHT:
      n_children = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self));
      if (n_children - column_pos == 1)
        return NULL;
      column = IDE_GRID_COLUMN (dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), column_pos + 1));
      stack = IDE_FRAME (dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (column), 0));
      page = ide_frame_get_visible_child (stack);
      break;

    case GTK_DIR_UP:
      if (stack_pos == 0)
        return NULL;
      stack = IDE_FRAME (dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (column), stack_pos - 1));
      page = ide_frame_get_visible_child (stack);
      break;

    case GTK_DIR_LEFT:
      if (column_pos == 0)
        return NULL;
      column = IDE_GRID_COLUMN (dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), column_pos - 1));
      stack = IDE_FRAME (dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (column), 0));
      page = ide_frame_get_visible_child (stack);
      break;

    case GTK_DIR_TAB_FORWARD:
      if (!ide_grid_focus_neighbor (self, GTK_DIR_DOWN) &&
          !ide_grid_focus_neighbor (self, GTK_DIR_RIGHT))
        {
          column = IDE_GRID_COLUMN (dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), 0));
          stack = IDE_FRAME (dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (column), 0));
          page = ide_frame_get_visible_child (stack);
        }
      break;

    case GTK_DIR_TAB_BACKWARD:
      if (!ide_grid_focus_neighbor (self, GTK_DIR_UP) &&
          !ide_grid_focus_neighbor (self, GTK_DIR_LEFT))
        {
          n_children = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self));
          column = IDE_GRID_COLUMN (dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), n_children - 1));
          stack = IDE_FRAME (dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (column), 0));
          page = ide_frame_get_visible_child (stack);
        }
      break;

    default:
      g_assert_not_reached ();
    }

  if (page != NULL)
    gtk_widget_child_focus (GTK_WIDGET (page), GTK_DIR_TAB_FORWARD);

  return page;
}
