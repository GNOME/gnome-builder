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

#include "ide-layout-grid.h"
#include "ide-layout-private.h"

typedef struct
{
  DzlSignalGroup *toplevel_signals;
  GQueue          focus_column;
} IdeLayoutGridPrivate;

enum {
  PROP_0,
  PROP_CURRENT_COLUMN,
  PROP_CURRENT_STACK,
  N_PROPS
};

enum {
  CREATE_STACK,
  N_SIGNALS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeLayoutGrid, ide_layout_grid, DZL_TYPE_MULTI_PANED)

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
  g_assert (IDE_IS_LAYOUT_GRID (self));
  g_assert (!widget || GTK_IS_WIDGET (widget));
  g_assert (GTK_IS_WINDOW (toplevel));

  if (widget != NULL)
    {
      if (gtk_widget_is_ancestor (widget, GTK_WIDGET (self)))
        {
          GtkWidget *column = gtk_widget_get_ancestor (widget, IDE_TYPE_LAYOUT_GRID_COLUMN);

          if (column != NULL)
            ide_layout_grid_set_current_column (self, IDE_LAYOUT_GRID_COLUMN (column));
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
      g_queue_push_head (&priv->focus_column, widget);
      GTK_CONTAINER_CLASS (ide_layout_grid_parent_class)->add (container, widget);
      ide_layout_grid_set_current_column (self, IDE_LAYOUT_GRID_COLUMN (widget));
      _ide_layout_grid_column_update_actions (IDE_LAYOUT_GRID_COLUMN (widget));
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
                         "The most recent focused IdeLayoutStack",
                         IDE_TYPE_LAYOUT_STACK,
                         (G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "idelayoutgrid");

  signals [CREATE_STACK] =
    g_signal_new ("create-stack",
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

  priv->toplevel_signals = dzl_signal_group_new (GTK_TYPE_WINDOW);

  dzl_signal_group_connect_object (priv->toplevel_signals,
                                   "set-focus",
                                   G_CALLBACK (ide_layout_grid_after_set_focus),
                                   self,
                                   G_CONNECT_SWAPPED | G_CONNECT_AFTER);
}

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
  GtkWidget *column = NULL;
  IdeLayoutStack *stack;

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

  g_assert (IDE_IS_LAYOUT_GRID_COLUMN (column));

  stack = ide_layout_grid_column_get_current_stack (IDE_LAYOUT_GRID_COLUMN (column));

  g_assert (IDE_IS_LAYOUT_STACK (stack));

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
  GtkWidget *ret;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), NULL);

  if (priv->focus_column.head != NULL)
    ret = priv->focus_column.head->data;
  else
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
