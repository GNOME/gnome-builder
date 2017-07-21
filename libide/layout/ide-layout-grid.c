/* ide-layout-grid.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-layout-grid"

#include "ide-object.h"

#include "layout/ide-layout-grid.h"
#include "layout/ide-layout-private.h"

/**
 * SECTION:ide-layout-grid
 * @title: IdeLayoutGrid
 * @short_description: A grid for #IdeLayoutView
 *
 * The #IdeLayoutGrid provides a grid of views that the user may
 * manipulate.
 *
 * Internally, this is implemented with #IdeLayoutGrid at the top
 * containing one or more of #IdeLayoutGridColumn. Those columns
 * contain one or more #IdeLayoutStack. The stack can contain many
 * #IdeLayoutView.
 *
 * #IdeLayoutGrid implements the #GListModel interface to simplify
 * the process of listing (with deduplication) the views that are
 * contianed within the #IdeLayoutGrid. If you would instead like
 * to see all possible views in the stack, use the
 * ide_layout_grid_foreach_view() API.
 *
 * Since: 3.26
 */

typedef struct
{
  /* Owned references */
  DzlSignalGroup *toplevel_signals;
  GQueue          focus_column;
  GArray         *stack_info;

  /*
   * This unowned reference is simply used to compare to a new focus
   * view to see if we have changed our current view. It is not to
   * be used directly, only for pointer comparison.
   */
  IdeLayoutView  *_last_focused_view;
} IdeLayoutGridPrivate;

typedef struct
{
  IdeLayoutStack *stack;
  guint           len;
} StackInfo;

enum {
  PROP_0,
  PROP_CURRENT_COLUMN,
  PROP_CURRENT_STACK,
  PROP_CURRENT_VIEW,
  N_PROPS
};

enum {
  CREATE_STACK,
  N_SIGNALS
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeLayoutGrid, ide_layout_grid, DZL_TYPE_MULTI_PANED,
                         G_ADD_PRIVATE (IdeLayoutGrid)
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
ide_layout_grid_update_actions (IdeLayoutGrid *self)
{
  guint n_children;

  g_assert (IDE_IS_LAYOUT_GRID (self));

  n_children = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self));

  for (guint i = 0; i < n_children; i++)
    {
      GtkWidget *column = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), i);

      g_assert (IDE_IS_LAYOUT_GRID_COLUMN (column));

      _ide_layout_grid_column_update_actions (IDE_LAYOUT_GRID_COLUMN (column));
    }
}

static IdeLayoutStack *
ide_layout_grid_real_create_stack (IdeLayoutGrid *self)
{
  return g_object_new (IDE_TYPE_LAYOUT_STACK,
                       "expand", TRUE,
                       "visible", TRUE,
                       NULL);
}

static GtkWidget *
ide_layout_grid_create_stack (IdeLayoutGrid *self)
{
  IdeLayoutStack *ret = NULL;

  g_assert (IDE_IS_LAYOUT_GRID (self));

  g_signal_emit (self, signals [CREATE_STACK], 0, &ret);
  g_return_val_if_fail (IDE_IS_LAYOUT_STACK (ret), NULL);
  return GTK_WIDGET (ret);
}

static GtkWidget *
ide_layout_grid_create_column (IdeLayoutGrid *self)
{
  GtkWidget *stack;

  g_assert (IDE_IS_LAYOUT_GRID (self));

  stack = ide_layout_grid_create_stack (self);

  if (stack != NULL)
    {
      GtkWidget *column = g_object_new (IDE_TYPE_LAYOUT_GRID_COLUMN,
                                        "visible", TRUE,
                                        NULL);
      gtk_container_add (GTK_CONTAINER (column), stack);
      return column;
    }

  return NULL;
}

static void
ide_layout_grid_after_set_focus (IdeLayoutGrid *self,
                                 GtkWidget     *widget,
                                 GtkWidget     *toplevel)
{
  IdeLayoutGridPrivate *priv = ide_layout_grid_get_instance_private (self);

  g_assert (IDE_IS_LAYOUT_GRID (self));
  g_assert (!widget || GTK_IS_WIDGET (widget));
  g_assert (GTK_IS_WINDOW (toplevel));

  if (widget != NULL)
    {
      GtkWidget *column = NULL;
      GtkWidget *view;

      if (gtk_widget_is_ancestor (widget, GTK_WIDGET (self)))
        {
          column = gtk_widget_get_ancestor (widget, IDE_TYPE_LAYOUT_GRID_COLUMN);

          if (column != NULL)
            ide_layout_grid_set_current_column (self, IDE_LAYOUT_GRID_COLUMN (column));
        }

      /*
       * self->_last_focused_view is an unowned reference, we only
       * use it for pointer comparison, nothing more.
       */
      view = gtk_widget_get_ancestor (widget, IDE_TYPE_LAYOUT_VIEW);
      if (view != (GtkWidget *)priv->_last_focused_view)
        {
          priv->_last_focused_view = (IdeLayoutView *)view;
          ide_object_notify_in_main (self, properties [PROP_CURRENT_VIEW]);

          if (view != NULL && column != NULL)
            {
              GtkWidget *stack;

              stack = gtk_widget_get_ancestor (GTK_WIDGET (view), IDE_TYPE_LAYOUT_STACK);
              if (stack != NULL)
                ide_layout_grid_column_set_current_stack (IDE_LAYOUT_GRID_COLUMN (column),
                                                          IDE_LAYOUT_STACK (stack));
            }
        }
    }
}

static void
ide_layout_grid_hierarchy_changed (GtkWidget *widget,
                                   GtkWidget *old_toplevel)
{
  IdeLayoutGrid *self = (IdeLayoutGrid *)widget;
  IdeLayoutGridPrivate *priv = ide_layout_grid_get_instance_private (self);
  GtkWidget *toplevel;

  g_assert (IDE_IS_LAYOUT_GRID (self));
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
   * we'll emit our ::create-stack signal to create that now. We do this here
   * to allow the consumer to connect to ::create-stack before adding the
   * widget to the hierarchy.
   */

  if (dzl_multi_paned_get_n_children (DZL_MULTI_PANED (widget)) == 0)
    {
      GtkWidget *column = ide_layout_grid_create_column (self);
      gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (column));
    }
}

static void
ide_layout_grid_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  IdeLayoutGrid *self = (IdeLayoutGrid *)container;
  IdeLayoutGridPrivate *priv = ide_layout_grid_get_instance_private (self);

  g_assert (IDE_IS_LAYOUT_GRID (self));
  g_assert (GTK_IS_WIDGET (widget));

  if (IDE_IS_LAYOUT_GRID_COLUMN (widget))
    {
      GList *children;

      /* Add our column to the grid */
      g_queue_push_head (&priv->focus_column, widget);
      GTK_CONTAINER_CLASS (ide_layout_grid_parent_class)->add (container, widget);
      ide_layout_grid_set_current_column (self, IDE_LAYOUT_GRID_COLUMN (widget));
      _ide_layout_grid_column_update_actions (IDE_LAYOUT_GRID_COLUMN (widget));

      /* Start monitoring all the stacks in the grid for views */
      children = gtk_container_get_children (GTK_CONTAINER (widget));
      for (const GList *iter = children; iter; iter = iter->next)
        if (IDE_IS_LAYOUT_STACK (iter->data))
          _ide_layout_grid_stack_added (self, iter->data);
      g_list_free (children);
    }
  else if (IDE_IS_LAYOUT_STACK (widget))
    {
      IdeLayoutGridColumn *column;

      column = ide_layout_grid_get_current_column (self);
      gtk_container_add (GTK_CONTAINER (column), widget);
      ide_layout_grid_set_current_column (self, column);
    }
  else if (IDE_IS_LAYOUT_VIEW (widget))
    {
      IdeLayoutGridColumn *column = NULL;
      guint n_columns;

      /* If we have an empty layout stack, we'll prefer to add the
       * view to that. If we don't find an empty stack, we'll add
       * the view to the most recently focused stack.
       */

      n_columns = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self));

      for (guint i = 0; i < n_columns; i++)
        {
          GtkWidget *ele = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), i);

          g_assert (IDE_IS_LAYOUT_GRID_COLUMN (ele));

          if (_ide_layout_grid_column_is_empty (IDE_LAYOUT_GRID_COLUMN (ele)))
            {
              column = IDE_LAYOUT_GRID_COLUMN (ele);
              break;
            }
        }

      if (column == NULL)
        column = ide_layout_grid_get_current_column (self);

      g_assert (IDE_IS_LAYOUT_GRID_COLUMN (column));

      gtk_container_add (GTK_CONTAINER (column), widget);
    }
  else
    {
      g_warning ("%s must be one of IdeLayoutStack, IdeLayoutView, or IdeLayoutGrid",
                 G_OBJECT_TYPE_NAME (self));
      return;
    }

  ide_layout_grid_update_actions (self);
}

static void
ide_layout_grid_remove (GtkContainer *container,
                        GtkWidget    *widget)
{
  IdeLayoutGrid *self = (IdeLayoutGrid *)container;
  IdeLayoutGridPrivate *priv = ide_layout_grid_get_instance_private (self);
  gboolean notify = FALSE;

  g_assert (IDE_IS_LAYOUT_GRID (self));
  g_assert (IDE_IS_LAYOUT_GRID_COLUMN (widget));

  notify = g_queue_peek_head (&priv->focus_column) == (gpointer)widget;
  g_queue_remove (&priv->focus_column, widget);

  GTK_CONTAINER_CLASS (ide_layout_grid_parent_class)->remove (container, widget);

  ide_layout_grid_update_actions (self);

  if (notify)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CURRENT_COLUMN]);
}

static void
ide_layout_grid_finalize (GObject *object)
{
  IdeLayoutGrid *self = (IdeLayoutGrid *)object;
  IdeLayoutGridPrivate *priv = ide_layout_grid_get_instance_private (self);

  g_assert (IDE_IS_LAYOUT_GRID (self));
  g_assert (priv->focus_column.head == NULL);
  g_assert (priv->focus_column.tail == NULL);
  g_assert (priv->focus_column.length == 0);

  g_clear_pointer (&priv->stack_info, g_array_unref);
  g_clear_object (&priv->toplevel_signals);

  G_OBJECT_CLASS (ide_layout_grid_parent_class)->finalize (object);
}

static void
ide_layout_grid_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeLayoutGrid *self = IDE_LAYOUT_GRID (object);

  switch (prop_id)
    {
    case PROP_CURRENT_COLUMN:
      g_value_set_object (value, ide_layout_grid_get_current_column (self));
      break;

    case PROP_CURRENT_STACK:
      g_value_set_object (value, ide_layout_grid_get_current_stack (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_layout_grid_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeLayoutGrid *self = IDE_LAYOUT_GRID (object);

  switch (prop_id)
    {
    case PROP_CURRENT_COLUMN:
      ide_layout_grid_set_current_column (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_layout_grid_class_init (IdeLayoutGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = ide_layout_grid_finalize;
  object_class->get_property = ide_layout_grid_get_property;
  object_class->set_property = ide_layout_grid_set_property;

  widget_class->hierarchy_changed = ide_layout_grid_hierarchy_changed;

  container_class->add = ide_layout_grid_add;
  container_class->remove = ide_layout_grid_remove;

  klass->create_stack = ide_layout_grid_real_create_stack;

  properties [PROP_CURRENT_COLUMN] =
    g_param_spec_object ("current-column",
                         "Current Column",
                         "The most recently focused grid column",
                         IDE_TYPE_LAYOUT_GRID_COLUMN,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CURRENT_STACK] =
    g_param_spec_object ("current-stack",
                         "Current Stack",
                         "The most recently focused IdeLayoutStack",
                         IDE_TYPE_LAYOUT_STACK,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CURRENT_VIEW] =
    g_param_spec_object ("current-view",
                         "Current View",
                         "The most recently focused IdeLayoutView",
                         IDE_TYPE_LAYOUT_VIEW,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "idelayoutgrid");

  /**
   * IdeLayoutGrid::create-stack:
   * @self: an #IdeLayoutGrid
   *
   * Creates a new stack to be added to the grid.
   *
   * Returns: (transfer full): A newly created #IdeLayoutStack
   */
  signals [CREATE_STACK] =
    g_signal_new (g_intern_static_string ("create-stack"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeLayoutGridClass, create_stack),
                  g_signal_accumulator_first_wins, NULL, NULL,
                  IDE_TYPE_LAYOUT_STACK, 0);
}

static void
ide_layout_grid_init (IdeLayoutGrid *self)
{
  IdeLayoutGridPrivate *priv = ide_layout_grid_get_instance_private (self);

  priv->stack_info = g_array_new (FALSE, FALSE, sizeof (StackInfo));

  priv->toplevel_signals = dzl_signal_group_new (GTK_TYPE_WINDOW);

  dzl_signal_group_connect_object (priv->toplevel_signals,
                                   "set-focus",
                                   G_CALLBACK (ide_layout_grid_after_set_focus),
                                   self,
                                   G_CONNECT_SWAPPED | G_CONNECT_AFTER);
}

/**
 * ide_layout_grid_new:
 *
 * Creates a new #IdeLayoutGrid.
 *
 * Returns: (transfer full): A newly created #IdeLayoutGrid
 *
 * Since: 3.26
 */
GtkWidget *
ide_layout_grid_new (void)
{
  return g_object_new (IDE_TYPE_LAYOUT_GRID, NULL);
}

/**
 * ide_layout_grid_get_current_stack:
 * @self: a #IdeLayoutGrid
 *
 * Gets the most recently focused stack. This is useful when you want to open
 * a document on the stack the user last focused.
 *
 * Returns: (transfer none) (nullable): A #IdeLayoutStack or %NULL.
 *
 * Since: 3.26
 */
IdeLayoutStack *
ide_layout_grid_get_current_stack (IdeLayoutGrid *self)
{
  IdeLayoutGridColumn *column;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), NULL);

  column = ide_layout_grid_get_current_column (self);
  if (column != NULL)
    return ide_layout_grid_column_get_current_stack (column);

  return NULL;
}

/**
 * ide_layout_grid_get_nth_column:
 * @self: a #IdeLayoutGrid
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
 * Returns: (transfer none): An #IdeLayoutGridColumn.
 */
IdeLayoutGridColumn *
ide_layout_grid_get_nth_column (IdeLayoutGrid *self,
                                gint           nth)
{
  GtkWidget *column;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), NULL);

  if (nth < 0)
    {
      column = ide_layout_grid_create_column (self);
      gtk_container_add_with_properties (GTK_CONTAINER (self), column,
                                         "index", 0,
                                         NULL);
    }
  else if (nth >= dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self)))
    {
      column = ide_layout_grid_create_column (self);
      gtk_container_add (GTK_CONTAINER (self), column);
    }
  else
    {
      column = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), nth);
    }

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID_COLUMN (column), NULL);

  return IDE_LAYOUT_GRID_COLUMN (column);
}

/*
 * _ide_layout_grid_get_nth_stack:
 *
 * This will get the @nth stack. If it does not yet exist,
 * it will be created.
 *
 * If nth == -1, a new stack will be created at index 0.
 *
 * If nth >= the number of stacks, a new stack will be created
 * at the end of the grid.
 *
 * Returns: (not nullable) (transfer none): An #IdeLayoutStack.
 */
IdeLayoutStack *
_ide_layout_grid_get_nth_stack (IdeLayoutGrid *self,
                                gint           nth)
{
  IdeLayoutGridColumn *column;
  IdeLayoutStack *stack;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), NULL);

  column = ide_layout_grid_get_nth_column (self, nth);
  stack = ide_layout_grid_column_get_current_stack (IDE_LAYOUT_GRID_COLUMN (column));

  g_return_val_if_fail (IDE_IS_LAYOUT_STACK (stack), NULL);

  return stack;
}

/**
 * _ide_layout_grid_get_nth_stack_for_column:
 * @self: an #IdeLayoutGrid
 * @column: an #IdeLayoutGridColumn
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
 * Returns: (not nullable) (transfer none): An #IdeLayoutStack.
 */
IdeLayoutStack *
_ide_layout_grid_get_nth_stack_for_column (IdeLayoutGrid       *self,
                                           IdeLayoutGridColumn *column,
                                           gint                 nth)
{
  GtkWidget *stack;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), NULL);
  g_return_val_if_fail (IDE_IS_LAYOUT_GRID_COLUMN (column), NULL);
  g_return_val_if_fail (gtk_widget_get_parent (GTK_WIDGET (column)) == GTK_WIDGET (self), NULL);

  if (nth < 0)
    {
      stack = ide_layout_grid_create_stack (self);
      gtk_container_add_with_properties (GTK_CONTAINER (column), stack,
                                         "index", 0,
                                         NULL);
    }
  else if (nth >= dzl_multi_paned_get_n_children (DZL_MULTI_PANED (column)))
    {
      stack = ide_layout_grid_create_stack (self);
      gtk_container_add (GTK_CONTAINER (self), stack);
    }
  else
    {
      stack = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (column), nth);
    }

  g_assert (IDE_IS_LAYOUT_STACK (stack));

  return IDE_LAYOUT_STACK (stack);
}

/**
 * ide_layout_grid_get_current_column:
 * @self: a #IdeLayoutGrid
 *
 * Gets the most recently focused column of the grid.
 *
 * Returns: (transfer none) (not nullable): An #IdeLayoutGridColumn
 */
IdeLayoutGridColumn *
ide_layout_grid_get_current_column (IdeLayoutGrid *self)
{
  IdeLayoutGridPrivate *priv = ide_layout_grid_get_instance_private (self);
  GtkWidget *ret = NULL;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), NULL);

  if (priv->focus_column.head != NULL)
    ret = priv->focus_column.head->data;
  else if (dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self)) > 0)
    ret = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), 0);

  if (ret == NULL)
    {
      ret = ide_layout_grid_create_column (self);
      gtk_container_add (GTK_CONTAINER (self), ret);
    }

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID_COLUMN (ret), NULL);

  return IDE_LAYOUT_GRID_COLUMN (ret);
}

/**
 * ide_layout_grid_set_current_column:
 * @self: an #IdeLayoutGrid
 * @column: (nullable): an #IdeLayoutGridColumn or %NULL
 *
 * Sets the current column for the grid. Generally this is automatically
 * updated for you when the focus changes within the workbench.
 *
 * @column can be %NULL out of convenience.
 */
void
ide_layout_grid_set_current_column (IdeLayoutGrid       *self,
                                    IdeLayoutGridColumn *column)
{
  IdeLayoutGridPrivate *priv = ide_layout_grid_get_instance_private (self);
  GList *iter;

  g_return_if_fail (IDE_IS_LAYOUT_GRID (self));
  g_return_if_fail (!column || IDE_IS_LAYOUT_GRID_COLUMN (column));

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
      ide_layout_grid_update_actions (self);
      return;
    }

  g_warning ("%s does not contain %s",
             G_OBJECT_TYPE_NAME (self), G_OBJECT_TYPE_NAME (column));
}

/**
 * ide_layout_grid_get_current_view:
 * @self: a #IdeLayoutGrid
 *
 * Gets the most recent view used by the user as determined by tracking
 * the window focus.
 *
 * Returns: (transfer none): An #IdeLayoutView or %NULL
 *
 * Since: 3.26
 */
IdeLayoutView *
ide_layout_grid_get_current_view (IdeLayoutGrid *self)
{
  IdeLayoutStack *stack;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), NULL);

  stack = ide_layout_grid_get_current_stack (self);

  if (stack != NULL)
    return ide_layout_stack_get_visible_child (stack);

  return NULL;
}

static void
collect_views (GtkWidget *widget,
               GPtrArray *ar)
{
  if (IDE_IS_LAYOUT_VIEW (widget))
    g_ptr_array_add (ar, widget);
}

/**
 * ide_layout_grid_foreach_view:
 * @self: a #IdeLayoutGrid
 * @callback: (scope call) (closure user_data): A callback for each view
 * @user_data: user data for @callback
 *
 * This function will call @callback for every view found in @self.
 *
 * Since: 3.26
 */
void
ide_layout_grid_foreach_view (IdeLayoutGrid *self,
                              GtkCallback    callback,
                              gpointer       user_data)
{
  g_autoptr(GPtrArray) views = NULL;
  guint n_columns;

  g_return_if_fail (IDE_IS_LAYOUT_GRID (self));
  g_return_if_fail (callback != NULL);

  views = g_ptr_array_new ();

  n_columns = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self));

  for (guint i = 0; i < n_columns; i++)
    {
      GtkWidget *column = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), i);
      guint n_stacks;

      g_assert (IDE_IS_LAYOUT_GRID_COLUMN (column));

      n_stacks = dzl_multi_paned_get_n_children (DZL_MULTI_PANED (column));

      for (guint j = 0; j < n_stacks; j++)
        {
          GtkWidget *stack = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (column), j);

          g_assert (IDE_IS_LAYOUT_STACK (stack));

          ide_layout_stack_foreach_view (IDE_LAYOUT_STACK (stack),
                                         (GtkCallback) collect_views,
                                         views);
        }
    }

  for (guint i = 0; i < views->len; i++)
    callback (g_ptr_array_index (views, i), user_data);
}

static GType
ide_layout_grid_get_item_type (GListModel *model)
{
  return IDE_TYPE_LAYOUT_VIEW;
}

static guint
ide_layout_grid_get_n_items (GListModel *model)
{
  IdeLayoutGrid *self = (IdeLayoutGrid *)model;
  IdeLayoutGridPrivate *priv = ide_layout_grid_get_instance_private (self);
  guint n_items = 0;

  g_assert (IDE_IS_LAYOUT_GRID (self));

  for (guint i = 0; i < priv->stack_info->len; i++)
    n_items += g_array_index (priv->stack_info, StackInfo, i).len;

  return n_items;
}

static gpointer
ide_layout_grid_get_item (GListModel *model,
                          guint       position)
{
  IdeLayoutGrid *self = (IdeLayoutGrid *)model;
  IdeLayoutGridPrivate *priv = ide_layout_grid_get_instance_private (self);

  g_assert (IDE_IS_LAYOUT_GRID (self));
  g_assert (position < ide_layout_grid_get_n_items (model));

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
  iface->get_item_type = ide_layout_grid_get_item_type;
  iface->get_n_items = ide_layout_grid_get_n_items;
  iface->get_item = ide_layout_grid_get_item;
}

static void
ide_layout_grid_stack_items_changed (IdeLayoutGrid  *self,
                                     guint           position,
                                     guint           removed,
                                     guint           added,
                                     IdeLayoutStack *stack)
{
  IdeLayoutGridPrivate *priv = ide_layout_grid_get_instance_private (self);
  guint real_position = 0;

  g_assert (IDE_IS_LAYOUT_GRID (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

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

          ide_object_notify_in_main (G_OBJECT (self), properties [PROP_CURRENT_VIEW]);

          return;
        }

      real_position += info->len;
    }

  g_warning ("Failed to locate %s within %s",
             G_OBJECT_TYPE_NAME (stack), G_OBJECT_TYPE_NAME (self));
}

void
_ide_layout_grid_stack_added (IdeLayoutGrid  *self,
                              IdeLayoutStack *stack)
{
  IdeLayoutGridPrivate *priv = ide_layout_grid_get_instance_private (self);
  StackInfo info = { 0 };
  guint n_items;

  g_return_if_fail (IDE_IS_LAYOUT_GRID (self));
  g_return_if_fail (IDE_IS_LAYOUT_STACK (stack));
  g_return_if_fail (G_IS_LIST_MODEL (stack));

  info.stack = stack;
  info.len = 0;

  g_array_append_val (priv->stack_info, info);

  g_signal_connect_object (stack,
                           "items-changed",
                           G_CALLBACK (ide_layout_grid_stack_items_changed),
                           self,
                           G_CONNECT_SWAPPED);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (stack));
  ide_layout_grid_stack_items_changed (self, 0, 0, n_items, stack);
}

void
_ide_layout_grid_stack_removed (IdeLayoutGrid  *self,
                                IdeLayoutStack *stack)
{
  IdeLayoutGridPrivate *priv = ide_layout_grid_get_instance_private (self);
  guint position = 0;

  g_return_if_fail (IDE_IS_LAYOUT_GRID (self));
  g_return_if_fail (IDE_IS_LAYOUT_STACK (stack));

  g_signal_handlers_disconnect_by_func (stack,
                                        G_CALLBACK (ide_layout_grid_stack_items_changed),
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
count_views_cb (GtkWidget *widget,
                gpointer   data)
{
  (*(guint *)data)++;
}

guint
ide_layout_grid_count_views (IdeLayoutGrid *self)
{
  guint count = 0;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), 0);

  ide_layout_grid_foreach_view (self, count_views_cb, &count);

  return count;
}
