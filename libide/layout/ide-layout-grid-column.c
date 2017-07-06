/* ide-layout-grid-column.c
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

#define G_LOG_DOMAIN "ide-layout-grid-column"

#include "ide-layout-grid-column.h"
#include "ide-layout-private.h"
#include "ide-layout-view.h"

struct _IdeLayoutGridColumn
{
  DzlMultiPaned parent_instance;
  GQueue        focus_stack;
};

typedef struct
{
  GList *stacks;
  GTask *backpointer;
} TryCloseState;

G_DEFINE_TYPE (IdeLayoutGridColumn, ide_layout_grid_column, DZL_TYPE_MULTI_PANED)

static void ide_layout_grid_column_try_close_pump (GTask *task);

enum {
  PROP_0,
  PROP_CURRENT_STACK,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
try_close_state_free (gpointer data)
{
  TryCloseState *state = data;

  g_assert (state != NULL);

  g_list_free_full (state->stacks, g_object_unref);
  state->stacks = NULL;
  state->backpointer = NULL;

  g_slice_free (TryCloseState, state);
}

static void
ide_layout_grid_column_add (GtkContainer *container,
                            GtkWidget    *widget)
{
  IdeLayoutGridColumn *self = (IdeLayoutGridColumn *)container;

  g_assert (IDE_IS_LAYOUT_GRID_COLUMN (self));

  if (IDE_IS_LAYOUT_VIEW (widget))
    {
      GtkWidget *child;

      g_assert (dzl_multi_paned_get_n_children (DZL_MULTI_PANED (container)) > 0);

      child = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (container), 0);
      gtk_container_add (GTK_CONTAINER (child), widget);
    }
  else if (IDE_IS_LAYOUT_STACK (widget))
    {
      GtkWidget *grid;

      g_queue_push_head (&self->focus_stack, widget);
      GTK_CONTAINER_CLASS (ide_layout_grid_column_parent_class)->add (container, widget);

      if (IDE_IS_LAYOUT_GRID (grid = gtk_widget_get_parent (GTK_WIDGET (self))))
        _ide_layout_grid_stack_added (IDE_LAYOUT_GRID (grid), IDE_LAYOUT_STACK (widget));
    }
  else
    {
      g_warning ("%s only supports adding IdeLayoutView or IdeLayoutStack",
                 G_OBJECT_TYPE_NAME (self));
      return;
    }
}

static void
ide_layout_grid_column_remove (GtkContainer *container,
                               GtkWidget    *widget)
{
  IdeLayoutGridColumn *self = (IdeLayoutGridColumn *)container;
  GtkWidget *grid;

  g_assert (IDE_IS_LAYOUT_GRID_COLUMN (self));
  g_assert (IDE_IS_LAYOUT_STACK (widget));

  if (IDE_IS_LAYOUT_GRID (grid = gtk_widget_get_parent (GTK_WIDGET (self))))
    _ide_layout_grid_stack_removed (IDE_LAYOUT_GRID (grid), IDE_LAYOUT_STACK (widget));

  g_queue_remove (&self->focus_stack, widget);

  GTK_CONTAINER_CLASS (ide_layout_grid_column_parent_class)->remove (container, widget);
}

static void
ide_layout_grid_column_finalize (GObject *object)
{
  IdeLayoutGridColumn *self = (IdeLayoutGridColumn *)object;

  g_assert (self->focus_stack.head == NULL);
  g_assert (self->focus_stack.tail == NULL);
  g_assert (self->focus_stack.length == 0);

  G_OBJECT_CLASS (ide_layout_grid_column_parent_class)->finalize (object);
}

static void
ide_layout_grid_column_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  IdeLayoutGridColumn *self = IDE_LAYOUT_GRID_COLUMN (object);

  switch (prop_id)
    {
    case PROP_CURRENT_STACK:
      g_value_set_object (value, ide_layout_grid_column_get_current_stack (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_layout_grid_column_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  IdeLayoutGridColumn *self = IDE_LAYOUT_GRID_COLUMN (object);

  switch (prop_id)
    {
    case PROP_CURRENT_STACK:
      ide_layout_grid_column_set_current_stack (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_layout_grid_column_class_init (IdeLayoutGridColumnClass *klass)
{
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_layout_grid_column_finalize;
  object_class->get_property = ide_layout_grid_column_get_property;
  object_class->set_property = ide_layout_grid_column_set_property;

  container_class->add = ide_layout_grid_column_add;
  container_class->remove = ide_layout_grid_column_remove;

  properties [PROP_CURRENT_STACK] =
    g_param_spec_object ("current-stack",
                         "Current Stack",
                         "The most recently focused stack within the column",
                         IDE_TYPE_LAYOUT_STACK,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "idelayoutgridcolumn");
}

static void
ide_layout_grid_column_init (IdeLayoutGridColumn *self)
{
  _ide_layout_grid_column_init_actions (self);
}

GtkWidget *
ide_layout_grid_column_new (void)
{
  return g_object_new (IDE_TYPE_LAYOUT_GRID_COLUMN, NULL);
}

static void
ide_layout_grid_column_try_close_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeLayoutStack *stack = (IdeLayoutStack *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_LAYOUT_STACK (stack));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_layout_stack_agree_to_close_finish (stack, result, &error))
    {
      g_debug ("Cannot close stack now due to: %s", error->message);
      gtk_widget_grab_focus (GTK_WIDGET (stack));
      g_task_return_boolean (task, FALSE);
      return;
    }

  gtk_widget_destroy (GTK_WIDGET (stack));

  ide_layout_grid_column_try_close_pump (g_steal_pointer (&task));
}

static void
ide_layout_grid_column_try_close_pump (GTask *_task)
{
  g_autoptr(GTask) task = _task;
  g_autoptr(IdeLayoutStack) stack = NULL;
  TryCloseState *state;
  GCancellable *cancellable;

  g_assert (G_IS_TASK (task));

  state = g_task_get_task_data (task);
  g_assert (state != NULL);
  g_assert (state->backpointer == task);

  if (state->stacks == NULL)
    {
      IdeLayoutGridColumn *self = g_task_get_source_object (task);

      g_assert (IDE_IS_LAYOUT_GRID_COLUMN (self));
      gtk_widget_destroy (GTK_WIDGET (self));
      g_task_return_boolean (task, TRUE);
      return;
    }

  stack = state->stacks->data;
  state->stacks = g_list_remove (state->stacks, stack);
  g_assert (IDE_IS_LAYOUT_STACK (stack));

  cancellable = g_task_get_cancellable (task);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_layout_stack_agree_to_close_async (stack,
                                         cancellable,
                                         ide_layout_grid_column_try_close_cb,
                                         g_steal_pointer (&task));
}

void
_ide_layout_grid_column_try_close (IdeLayoutGridColumn *self)
{
  TryCloseState state = { 0 };
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_LAYOUT_GRID_COLUMN (self));

  state.stacks = gtk_container_get_children (GTK_CONTAINER (self));

  if (state.stacks == NULL)
    {
      /* Implausible and should not happen because we should always
       * have a stack inside the grid when the action is activated.
       */
      gtk_widget_destroy (GTK_WIDGET (self));
      g_return_if_reached ();
    }

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_source_tag (task, _ide_layout_grid_column_try_close);
  g_task_set_priority (task, G_PRIORITY_LOW);

  g_list_foreach (state.stacks, (GFunc)g_object_ref, NULL);
  state.backpointer = task;
  g_task_set_task_data (task, g_slice_dup (TryCloseState, &state), try_close_state_free);

  ide_layout_grid_column_try_close_pump (g_steal_pointer (&task));
}

gboolean
_ide_layout_grid_column_is_empty (IdeLayoutGridColumn *self)
{
  g_return_val_if_fail (IDE_IS_LAYOUT_GRID_COLUMN (self), FALSE);

  /*
   * Check if we only have a single stack and it is empty.
   * That means we are in our "initial/empty" state.
   */

  if (dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self)) == 1)
    {
      GtkWidget *child = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), 0);

      g_assert (IDE_IS_LAYOUT_STACK (child));

      return !ide_layout_stack_get_has_view (IDE_LAYOUT_STACK (child));
    }

  return FALSE;
}

/**
 * ide_layout_grid_column_get_current_stack:
 * @self: a #IdeLayoutGridColumn
 *
 * Gets the most recently focused stack. If no stack has been added, then
 * %NULL is returned.
 *
 * Returns: (transfer none) (nullable): an #IdeLayoutStack or %NULL.
 */
IdeLayoutStack *
ide_layout_grid_column_get_current_stack (IdeLayoutGridColumn *self)
{
  g_return_val_if_fail (IDE_IS_LAYOUT_GRID_COLUMN (self), NULL);

  return self->focus_stack.head ? self->focus_stack.head->data : NULL;
}

void
ide_layout_grid_column_set_current_stack (IdeLayoutGridColumn *self,
                                          IdeLayoutStack      *stack)
{
  GList *iter;

  g_return_if_fail (IDE_IS_LAYOUT_GRID_COLUMN (self));
  g_return_if_fail (!stack || IDE_IS_LAYOUT_STACK (stack));

  /* If there is nothing to do, short-circuit. */
  if (stack == NULL ||
      (self->focus_stack.head != NULL &&
       self->focus_stack.head->data == (gpointer)stack))
    return;

  /*
   * If we are already in the stack, we can just move our element
   * without having to setup signal handling.
   */
  if (NULL != (iter = g_queue_find (&self->focus_stack, stack)))
    {
      g_queue_unlink (&self->focus_stack, iter);
      g_queue_push_head_link (&self->focus_stack, iter);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CURRENT_STACK]);
      return;
    }

  g_warning ("%s was not found within %s",
             G_OBJECT_TYPE_NAME (stack), G_OBJECT_TYPE_NAME (self));
}
