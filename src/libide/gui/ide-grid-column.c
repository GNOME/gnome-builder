/* ide-grid-column.c
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

#define G_LOG_DOMAIN "ide-grid-column"

#include "config.h"

#include <libide-core.h>
#include <libide-threading.h>

#include "ide-grid-column.h"
#include "ide-gui-private.h"
#include "ide-page.h"

struct _IdeGridColumn
{
  DzlMultiPaned parent_instance;
  GQueue        focus_stack;
};

typedef struct
{
  GList *stacks;
  IdeTask *backpointer;
} TryCloseState;

G_DEFINE_TYPE (IdeGridColumn, ide_grid_column, DZL_TYPE_MULTI_PANED)

static void ide_grid_column_try_close_pump (IdeTask *task);

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
ide_grid_column_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  IdeGridColumn *self = (IdeGridColumn *)container;

  g_assert (IDE_IS_GRID_COLUMN (self));

  if (IDE_IS_PAGE (widget))
    {
      GtkWidget *child;

      g_assert (dzl_multi_paned_get_n_children (DZL_MULTI_PANED (container)) > 0);

      child = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (container), 0);
      gtk_container_add (GTK_CONTAINER (child), widget);
    }
  else if (IDE_IS_FRAME (widget))
    {
      GtkWidget *grid;

      g_queue_push_head (&self->focus_stack, widget);
      GTK_CONTAINER_CLASS (ide_grid_column_parent_class)->add (container, widget);

      if (IDE_IS_GRID (grid = gtk_widget_get_parent (GTK_WIDGET (self))))
        _ide_grid_stack_added (IDE_GRID (grid), IDE_FRAME (widget));
    }
  else
    {
      g_warning ("%s only supports adding IdePage or IdeFrame",
                 G_OBJECT_TYPE_NAME (self));
      return;
    }
}

static void
ide_grid_column_remove (GtkContainer *container,
                        GtkWidget    *widget)
{
  IdeGridColumn *self = (IdeGridColumn *)container;
  GtkWidget *grid;
  gboolean notify;

  g_assert (IDE_IS_GRID_COLUMN (self));
  g_assert (IDE_IS_FRAME (widget));

  notify = g_queue_peek_head (&self->focus_stack) == (gpointer)widget;
  g_queue_remove (&self->focus_stack, widget);

  if (IDE_IS_GRID (grid = gtk_widget_get_parent (GTK_WIDGET (self))))
    _ide_grid_stack_removed (IDE_GRID (grid), IDE_FRAME (widget));

  GTK_CONTAINER_CLASS (ide_grid_column_parent_class)->remove (container, widget);

  if (notify)
    {
      GtkWidget *head = g_queue_peek_head (&self->focus_stack);

      if (head != NULL)
        gtk_widget_grab_focus (head);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CURRENT_STACK]);
    }
}

static void
ide_grid_column_grab_focus (GtkWidget *widget)
{
  IdeGridColumn *self = (IdeGridColumn *)widget;
  IdeFrame *stack;

  g_assert (IDE_IS_GRID_COLUMN (self));

  stack = ide_grid_column_get_current_stack (self);

  if (stack != NULL)
    gtk_widget_grab_focus (GTK_WIDGET (stack));
  else
    GTK_WIDGET_CLASS (ide_grid_column_parent_class)->grab_focus (widget);
}

static void
ide_grid_column_finalize (GObject *object)
{
#ifndef G_DISABLE_ASSERT
  IdeGridColumn *self = (IdeGridColumn *)object;

  g_assert (self->focus_stack.head == NULL);
  g_assert (self->focus_stack.tail == NULL);
  g_assert (self->focus_stack.length == 0);
#endif

  G_OBJECT_CLASS (ide_grid_column_parent_class)->finalize (object);
}

static void
ide_grid_column_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeGridColumn *self = IDE_GRID_COLUMN (object);

  switch (prop_id)
    {
    case PROP_CURRENT_STACK:
      g_value_set_object (value, ide_grid_column_get_current_stack (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_grid_column_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeGridColumn *self = IDE_GRID_COLUMN (object);

  switch (prop_id)
    {
    case PROP_CURRENT_STACK:
      ide_grid_column_set_current_stack (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_grid_column_class_init (IdeGridColumnClass *klass)
{
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_grid_column_finalize;
  object_class->get_property = ide_grid_column_get_property;
  object_class->set_property = ide_grid_column_set_property;

  widget_class->grab_focus = ide_grid_column_grab_focus;

  container_class->add = ide_grid_column_add;
  container_class->remove = ide_grid_column_remove;

  properties [PROP_CURRENT_STACK] =
    g_param_spec_object ("current-stack",
                         "Current Stack",
                         "The most recently focused stack within the column",
                         IDE_TYPE_FRAME,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "idegridcolumn");
}

static void
ide_grid_column_init (IdeGridColumn *self)
{
  _ide_grid_column_init_actions (self);
}

GtkWidget *
ide_grid_column_new (void)
{
  return g_object_new (IDE_TYPE_GRID_COLUMN, NULL);
}

static void
ide_grid_column_try_close_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdeFrame *stack = (IdeFrame *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_FRAME (stack));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_frame_agree_to_close_finish (stack, result, &error))
    {
      g_debug ("Cannot close stack now due to: %s", error->message);
      gtk_widget_grab_focus (GTK_WIDGET (stack));
      ide_task_return_boolean (task, FALSE);
      return;
    }

  gtk_widget_destroy (GTK_WIDGET (stack));

  ide_grid_column_try_close_pump (g_steal_pointer (&task));
}

static void
ide_grid_column_try_close_pump (IdeTask *_task)
{
  g_autoptr(IdeTask) task = _task;
  g_autoptr(IdeFrame) stack = NULL;
  TryCloseState *state;
  GCancellable *cancellable;

  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);
  g_assert (state != NULL);
  g_assert (state->backpointer == task);

  if (state->stacks == NULL)
    {
      IdeGridColumn *self = ide_task_get_source_object (task);

      g_assert (IDE_IS_GRID_COLUMN (self));
      gtk_widget_destroy (GTK_WIDGET (self));
      ide_task_return_boolean (task, TRUE);
      return;
    }

  stack = state->stacks->data;
  state->stacks = g_list_remove (state->stacks, stack);
  g_assert (IDE_IS_FRAME (stack));

  cancellable = ide_task_get_cancellable (task);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_frame_agree_to_close_async (stack,
                                         cancellable,
                                         ide_grid_column_try_close_cb,
                                         g_steal_pointer (&task));
}

void
_ide_grid_column_try_close (IdeGridColumn *self)
{
  TryCloseState state = { 0 };
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_GRID_COLUMN (self));

  state.stacks = gtk_container_get_children (GTK_CONTAINER (self));

  if (state.stacks == NULL)
    {
      /* Implausible and should not happen because we should always
       * have a stack inside the grid when the action is activated.
       */
      gtk_widget_destroy (GTK_WIDGET (self));
      g_return_if_reached ();
    }

  task = ide_task_new (self, NULL, NULL, NULL);
  ide_task_set_source_tag (task, _ide_grid_column_try_close);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  g_list_foreach (state.stacks, (GFunc)g_object_ref, NULL);
  state.backpointer = task;
  ide_task_set_task_data (task, g_slice_dup (TryCloseState, &state), try_close_state_free);

  ide_grid_column_try_close_pump (g_steal_pointer (&task));
}

gboolean
_ide_grid_column_is_empty (IdeGridColumn *self)
{
  g_return_val_if_fail (IDE_IS_GRID_COLUMN (self), FALSE);

  /*
   * Check if we only have a single stack and it is empty.
   * That means we are in our "initial/empty" state.
   */

  if (dzl_multi_paned_get_n_children (DZL_MULTI_PANED (self)) == 1)
    {
      GtkWidget *child = dzl_multi_paned_get_nth_child (DZL_MULTI_PANED (self), 0);

      g_assert (IDE_IS_FRAME (child));

      return !ide_frame_get_has_page (IDE_FRAME (child));
    }

  return FALSE;
}

/**
 * ide_grid_column_get_current_stack:
 * @self: a #IdeGridColumn
 *
 * Gets the most recently focused stack. If no stack has been added, then
 * %NULL is returned.
 *
 * Returns: (transfer none) (nullable): an #IdeFrame or %NULL.
 *
 * Since: 3.32
 */
IdeFrame *
ide_grid_column_get_current_stack (IdeGridColumn *self)
{
  g_return_val_if_fail (IDE_IS_GRID_COLUMN (self), NULL);

  return self->focus_stack.head ? self->focus_stack.head->data : NULL;
}

void
ide_grid_column_set_current_stack (IdeGridColumn *self,
                                   IdeFrame      *stack)
{
  GList *iter;

  g_return_if_fail (IDE_IS_GRID_COLUMN (self));
  g_return_if_fail (!stack || IDE_IS_FRAME (stack));

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
