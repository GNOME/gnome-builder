/* ide-layout-grid.c
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

#define G_LOG_DOMAIN "ide-layout-grid"

#include <glib/gi18n.h>

#include "ide-layout-grid.h"
#include "ide-layout-stack.h"
#include "ide-layout-stack-private.h"
#include "ide-layout-view.h"

struct _IdeLayoutGrid
{
  GtkBin          parent_instance;

  IdeLayoutStack *last_focus;
};

G_DEFINE_TYPE (IdeLayoutGrid, ide_layout_grid, GTK_TYPE_BIN)

static void ide_layout_grid_make_homogeneous (IdeLayoutGrid *self);

GtkWidget *
ide_layout_grid_new (void)
{
  return g_object_new (IDE_TYPE_LAYOUT_GRID, NULL);
}

static void
ide_layout_grid_remove_stack (IdeLayoutGrid  *self,
                              IdeLayoutStack *stack)
{
  GtkWidget *new_focus;
  GList *stacks;
  GList *iter;

  g_return_if_fail (IDE_IS_LAYOUT_GRID (self));
  g_return_if_fail (IDE_IS_LAYOUT_STACK (stack));

  stacks = ide_layout_grid_get_stacks (self);

  /* refuse to remove the stack if there is only one */
  if (g_list_length (stacks) == 1)
    return;

  new_focus = ide_layout_grid_get_stack_before (self, stack);
  if (!new_focus)
    new_focus = ide_layout_grid_get_stack_after (self, stack);

  for (iter = stacks; iter; iter = iter->next)
    {
      IdeLayoutStack *item = IDE_LAYOUT_STACK (iter->data);

      if (item == stack)
        {
          if (!iter->prev)
            {
              GtkWidget *paned;
              GtkWidget *child2;

              /*
               * This is the first stack in the grid. All we need to do to get
               * to a consistent state is to take the child2 paned and replace
               * our toplevel paned with it.
               */
              paned = gtk_bin_get_child (GTK_BIN (self));
              child2 = gtk_paned_get_child2 (GTK_PANED (paned));
              g_object_ref (child2);
              gtk_container_remove (GTK_CONTAINER (paned), child2);
              gtk_container_remove (GTK_CONTAINER (self), paned);
              gtk_container_add (GTK_CONTAINER (self), child2);
              g_object_unref (child2);
            }
          else if (!iter->next)
            {
              GtkWidget *paned;
              GtkWidget *grandparent;

              /*
               * This is the last stack in the grid. All we need to do to get
               * to a consistent state is remove our parent paned from the
               * grandparent.
               */
              paned = gtk_widget_get_parent (GTK_WIDGET (stack));
              grandparent = gtk_widget_get_parent (paned);
              gtk_container_remove (GTK_CONTAINER (grandparent), paned);
            }
          else if (iter->next && iter->prev)
            {
              GtkWidget *grandparent;
              GtkWidget *paned;
              GtkWidget *child2;

              /*
               * This stack is somewhere in the middle. All we need to do to
               * get into a consistent state is take our parent paneds child2
               * and put it in our parent's location.
               */
              paned = gtk_widget_get_parent (GTK_WIDGET (stack));
              grandparent = gtk_widget_get_parent (paned);
              child2 = gtk_paned_get_child2 (GTK_PANED (paned));
              g_object_ref (child2);
              gtk_container_remove (GTK_CONTAINER (paned), child2);
              gtk_container_remove (GTK_CONTAINER (grandparent), paned);
              gtk_container_add (GTK_CONTAINER (grandparent), child2);
              g_object_unref (child2);
            }
          else
            g_assert_not_reached ();

          ide_layout_grid_make_homogeneous (self);

          break;
        }
    }

  if (new_focus)
    gtk_widget_grab_focus (new_focus);

  g_list_free (stacks);
}

static GtkWidget *
ide_layout_grid_get_first_stack (IdeLayoutGrid *self)
{
  GtkWidget *child;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), NULL);

  child = gtk_bin_get_child (GTK_BIN (self));

  if (GTK_IS_PANED (child))
    {
      child = gtk_paned_get_child1 (GTK_PANED (child));
      if (IDE_IS_LAYOUT_STACK (child))
        return child;
    }

  return NULL;
}

static GtkWidget *
ide_layout_grid_get_last_stack (IdeLayoutGrid *self)
{
  GtkWidget *child;
  GtkWidget *child2;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), NULL);

  child = gtk_bin_get_child (GTK_BIN (self));

  while (GTK_IS_PANED (child) &&
         (child2 = gtk_paned_get_child2 (GTK_PANED (child))))
    child = child2;

  child = gtk_paned_get_child1 (GTK_PANED (child));

  if (IDE_IS_LAYOUT_STACK (child))
    return child;

  return NULL;
}

static void
ide_layout_grid_focus_neighbor (IdeLayoutGrid    *self,
                                GtkDirectionType  dir,
                                IdeLayoutStack   *stack)
{
  GtkWidget *active_view;
  GtkWidget *neighbor = NULL;

  g_return_if_fail (IDE_IS_LAYOUT_GRID (self));
  g_return_if_fail (IDE_IS_LAYOUT_STACK (stack));

  switch ((int)dir)
    {
    case GTK_DIR_UP:
    case GTK_DIR_TAB_BACKWARD:
      active_view = ide_layout_stack_get_active_view (stack);
      if (active_view && gtk_widget_child_focus (active_view, dir))
        break;
      /* fallthrough */
    case GTK_DIR_LEFT:
      neighbor = ide_layout_grid_get_stack_before (self, stack);
      if (!neighbor)
        neighbor = ide_layout_grid_get_last_stack (self);
      break;

    case GTK_DIR_DOWN:
    case GTK_DIR_TAB_FORWARD:
      active_view = ide_layout_stack_get_active_view (stack);
      if (active_view && gtk_widget_child_focus (active_view, dir))
        break;
      /* fallthrough */
    case GTK_DIR_RIGHT:
      neighbor = ide_layout_grid_get_stack_after (self, stack);
      if (!neighbor)
        neighbor = ide_layout_grid_get_first_stack (self);
      break;

    default:
      break;
    }

  if (neighbor != NULL)
    gtk_widget_grab_focus (neighbor);
}

static void
ide_layout_grid_focus_neighbor_action (GSimpleAction *action,
                                       GVariant      *param,
                                       gpointer       user_data)
{
  IdeLayoutGrid *self = user_data;
  GtkDirectionType dir;

  g_assert (IDE_IS_LAYOUT_GRID (self));

  dir = g_variant_get_int32 (param);

  if (self->last_focus)
    ide_layout_grid_focus_neighbor (self, dir, self->last_focus);
}

static void
ide_layout_grid_stack_empty (IdeLayoutGrid  *self,
                          IdeLayoutStack *stack)
{
  GList *stacks;

  g_return_if_fail (IDE_IS_LAYOUT_GRID (self));
  g_return_if_fail (IDE_IS_LAYOUT_STACK (stack));

  stacks = ide_layout_grid_get_stacks (self);

  g_assert (stacks != NULL);

  if (g_list_length (stacks) == 1)
    goto cleanup;

  ide_layout_grid_focus_neighbor (self, GTK_DIR_LEFT, stack);
  ide_layout_grid_remove_stack (self, stack);

cleanup:
  g_list_free (stacks);
}

static void
ide_layout_grid_stack_split (IdeLayoutGrid      *self,
                             IdeLayoutView      *view,
                             IdeLayoutGridSplit  split,
                             IdeLayoutStack     *stack)
{
  GtkWidget *target_stack = NULL;
  IdeLayoutView *target_view = NULL;

  g_assert (IDE_IS_LAYOUT_VIEW (view));
  g_assert (IDE_IS_LAYOUT_GRID (self));
  g_assert (IDE_IS_LAYOUT_STACK (stack));

  switch (split)
    {
    case IDE_LAYOUT_GRID_SPLIT_LEFT:
      target_view = ide_layout_view_create_split (view);
      if (target_view == NULL)
        return;

      target_stack = ide_layout_grid_get_stack_before (self, stack);
      if (target_stack == NULL)
        target_stack = ide_layout_grid_add_stack_before (self, stack);

      ide_layout_stack_add (GTK_CONTAINER (target_stack), GTK_WIDGET (target_view));
      ide_layout_stack_set_active_view (IDE_LAYOUT_STACK (target_stack), GTK_WIDGET (target_view));

      break;

    case IDE_LAYOUT_GRID_SPLIT_MOVE_LEFT:
      target_stack = ide_layout_grid_get_stack_before (self, stack);
      if (target_stack == NULL)
        target_stack = ide_layout_grid_add_stack_before (self, stack);

      g_object_ref (view);
      ide_layout_stack_remove (stack, GTK_WIDGET (view));
      ide_layout_stack_add (GTK_CONTAINER (target_stack), GTK_WIDGET (view));
      ide_layout_stack_set_active_view (IDE_LAYOUT_STACK (target_stack), GTK_WIDGET (view));
      g_object_unref (view);

      break;

    case IDE_LAYOUT_GRID_SPLIT_RIGHT:
      target_view = ide_layout_view_create_split (view);
      if (target_view == NULL)
        return;

      target_stack = ide_layout_grid_get_stack_after (self, stack);
      if (target_stack == NULL)
        target_stack = ide_layout_grid_add_stack_after (self, stack);

      ide_layout_stack_add (GTK_CONTAINER (target_stack), GTK_WIDGET (target_view));
      ide_layout_stack_set_active_view (IDE_LAYOUT_STACK (target_stack), GTK_WIDGET (target_view));

      break;

    case IDE_LAYOUT_GRID_SPLIT_MOVE_RIGHT:
      target_stack = ide_layout_grid_get_stack_after (self, stack);
      if (target_stack == NULL)
        target_stack = ide_layout_grid_add_stack_after (self, stack);

      g_object_ref (view);
      ide_layout_stack_remove (stack, GTK_WIDGET (view));
      ide_layout_stack_add (GTK_CONTAINER (target_stack), GTK_WIDGET (view));
      ide_layout_stack_set_active_view (IDE_LAYOUT_STACK (target_stack), GTK_WIDGET (view));
      g_object_unref (view);

      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static GtkPaned *
ide_layout_grid_create_paned (IdeLayoutGrid *self)
{
  return g_object_new (GTK_TYPE_PANED,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       "visible", TRUE,
                       NULL);
}

static IdeLayoutStack *
ide_layout_grid_create_stack (IdeLayoutGrid *self)
{
  IdeLayoutStack *stack;

  g_assert (IDE_IS_LAYOUT_GRID (self));

  stack = g_object_new (IDE_TYPE_LAYOUT_STACK,
                        "visible", TRUE,
                        NULL);

  g_signal_connect_object (stack,
                           "empty",
                           G_CALLBACK (ide_layout_grid_stack_empty),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (stack,
                           "split",
                           G_CALLBACK (ide_layout_grid_stack_split),
                           self,
                           G_CONNECT_SWAPPED);

  return stack;
}

static void
ide_layout_grid_make_homogeneous (IdeLayoutGrid *self)
{
  GtkWidget *child;
  GList *stacks;
  GList *iter;
  GtkAllocation alloc;
  guint count;
  guint position;
  gint handle_size = 1;

  g_return_if_fail (IDE_IS_LAYOUT_GRID (self));

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  child = gtk_bin_get_child (GTK_BIN (self));
  gtk_widget_style_get (child, "handle-size", &handle_size, NULL);

  stacks = ide_layout_grid_get_stacks (self);
  count = MAX (1, g_list_length (stacks));
  position = (alloc.width - (handle_size * (count - 1))) / count;

  for (iter = stacks; iter; iter = iter->next)
    {
      GtkWidget *parent;

      parent = gtk_widget_get_parent (iter->data);
      g_assert (GTK_IS_PANED (parent));

      gtk_paned_set_position (GTK_PANED (parent), position);
    }

  g_list_free (stacks);
}

/**
 * ide_layout_grid_get_stacks:
 *
 * Fetches all of the stacks in the grid. The resulting #GList should be
 * freed with g_list_free().
 *
 * Returns: (transfer container) (element-type Ide.LayoutStack): A #GList.
 */
GList *
ide_layout_grid_get_stacks (IdeLayoutGrid *self)
{
  GtkWidget *paned;
  GList *list = NULL;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), NULL);

  paned = gtk_bin_get_child (GTK_BIN (self));

  while (paned)
    {
      GtkWidget *stack;

      stack = gtk_paned_get_child1 (GTK_PANED (paned));

      if (IDE_IS_LAYOUT_STACK (stack))
        list = g_list_append (list, stack);

      paned = gtk_paned_get_child2 (GTK_PANED (paned));
    }

#ifndef IDE_DISABLE_TRACE
  {
    GList *iter;

    for (iter = list; iter; iter = iter->next)
      g_assert (IDE_IS_LAYOUT_STACK (iter->data));
  }
#endif

  return list;
}

/**
 * ide_layout_grid_add_stack_before:
 *
 * Returns: (transfer none) (type Ide.LayoutStack): The new view stack.
 */
GtkWidget *
ide_layout_grid_add_stack_before (IdeLayoutGrid  *self,
                                  IdeLayoutStack *stack)
{
  IdeLayoutStack *new_stack;
  GtkWidget *parent;
  GtkWidget *grandparent;
  GtkPaned *new_paned;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), NULL);

  new_paned = ide_layout_grid_create_paned (self);
  new_stack = ide_layout_grid_create_stack (self);
  gtk_container_add (GTK_CONTAINER (new_paned), GTK_WIDGET (new_stack));

  parent = gtk_widget_get_parent (GTK_WIDGET (stack));
  grandparent = gtk_widget_get_parent (GTK_WIDGET (parent));

  if (GTK_IS_PANED (grandparent))
    {
      g_object_ref (parent);
      gtk_container_remove (GTK_CONTAINER (grandparent), GTK_WIDGET (parent));
      gtk_container_add_with_properties (GTK_CONTAINER (grandparent),
                                         GTK_WIDGET (new_paned),
                                         "shrink", FALSE,
                                         "resize", TRUE,
                                         NULL);
      gtk_container_add_with_properties (GTK_CONTAINER (new_paned),
                                         GTK_WIDGET (parent),
                                         "shrink", FALSE,
                                         "resize", TRUE,
                                         NULL);
      g_object_unref (parent);
    }
  else if (IDE_IS_LAYOUT_GRID (grandparent))
    {
      g_object_ref (parent);
      gtk_container_remove (GTK_CONTAINER (grandparent), GTK_WIDGET (parent));
      gtk_container_add (GTK_CONTAINER (grandparent), GTK_WIDGET (new_paned));
      gtk_container_add_with_properties (GTK_CONTAINER (new_paned), parent,
                                         "shrink", FALSE,
                                         "resize", TRUE,
                                         NULL);
      g_object_unref (parent);
    }
  else
    g_assert_not_reached ();

  ide_layout_grid_make_homogeneous (self);

  return GTK_WIDGET (new_stack);
}

/**
 * ide_layout_grid_add_stack_after:
 *
 * Returns: (transfer none) (type Ide.LayoutStack): The new view stack.
 */
GtkWidget *
ide_layout_grid_add_stack_after (IdeLayoutGrid  *self,
                                 IdeLayoutStack *stack)
{
  IdeLayoutStack *new_stack;
  GtkWidget *parent;
  GtkPaned *new_paned;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), NULL);

  new_paned = ide_layout_grid_create_paned (self);
  new_stack = ide_layout_grid_create_stack (self);
  gtk_container_add (GTK_CONTAINER (new_paned), GTK_WIDGET (new_stack));

  parent = gtk_widget_get_parent (GTK_WIDGET (stack));

  if (GTK_IS_PANED (parent))
    {
      GtkWidget *child2;

      child2 = gtk_paned_get_child2 (GTK_PANED (parent));

      if (child2)
        {
          g_object_ref (child2);
          gtk_container_remove (GTK_CONTAINER (parent), child2);
        }

      gtk_container_add_with_properties (GTK_CONTAINER (parent),
                                         GTK_WIDGET (new_paned),
                                         "shrink", FALSE,
                                         "resize", TRUE,
                                         NULL);

      if (child2)
        {
          gtk_container_add_with_properties (GTK_CONTAINER (new_paned), child2,
                                             "shrink", FALSE,
                                             "resize", TRUE,
                                             NULL);
          g_object_unref (child2);
        }
    }
  else
    g_assert_not_reached ();

  ide_layout_grid_make_homogeneous (self);

  return GTK_WIDGET (new_stack);
}

/**
 * ide_layout_grid_get_stack_before:
 *
 * Returns: (nullable) (transfer none) (type Ide.LayoutStack): The view stack.
 */
GtkWidget *
ide_layout_grid_get_stack_before (IdeLayoutGrid  *self,
                                  IdeLayoutStack *stack)
{
  GtkWidget *parent;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), NULL);
  g_return_val_if_fail (IDE_IS_LAYOUT_STACK (stack), NULL);

  parent = gtk_widget_get_parent (GTK_WIDGET (stack));

  if (GTK_IS_PANED (parent))
    {
      parent = gtk_widget_get_parent (parent);
      if (GTK_IS_PANED (parent))
        return gtk_paned_get_child1 (GTK_PANED (parent));
    }

  return NULL;
}

/**
 * ide_layout_grid_get_stack_after:
 *
 * Returns: (nullable) (transfer none) (type Ide.LayoutStack): The view stack.
 */
GtkWidget *
ide_layout_grid_get_stack_after (IdeLayoutGrid  *self,
                              IdeLayoutStack *stack)
{
  GtkWidget *parent;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), NULL);
  g_return_val_if_fail (IDE_IS_LAYOUT_STACK (stack), NULL);

  parent = gtk_widget_get_parent (GTK_WIDGET (stack));

  if (GTK_IS_PANED (parent))
    {
      GtkWidget *child2;

      child2 = gtk_paned_get_child2 (GTK_PANED (parent));
      if (GTK_IS_PANED (child2))
        return gtk_paned_get_child1 (GTK_PANED (child2));
    }

  return NULL;
}

static void
ide_layout_grid_grab_focus (GtkWidget *widget)
{
  IdeLayoutGrid *self = (IdeLayoutGrid *)widget;
  GList *stacks;

  g_return_if_fail (IDE_IS_LAYOUT_GRID (self));

  if (self->last_focus)
    {
      gtk_widget_grab_focus (GTK_WIDGET (self->last_focus));
      return;
    }

  stacks = ide_layout_grid_get_stacks (self);
  if (stacks)
    gtk_widget_grab_focus (stacks->data);
  g_list_free (stacks);
}

static void
ide_layout_grid_set_focus (IdeLayoutGrid  *self,
                           IdeLayoutStack *stack)
{
  if (self->last_focus)
    {
      GtkStyleContext *style_context;

      style_context = gtk_widget_get_style_context (GTK_WIDGET (self->last_focus));
      gtk_style_context_remove_class (style_context, "focus-stack");
      ide_clear_weak_pointer (&self->last_focus);
    }

  if (stack != NULL)
    {
      GtkStyleContext *style_context;

      style_context = gtk_widget_get_style_context (GTK_WIDGET (stack));
      gtk_style_context_add_class (style_context, "focus-stack");
      ide_set_weak_pointer (&self->last_focus, stack);
    }
}

static void
ide_layout_grid_toplevel_set_focus (GtkWidget     *toplevel,
                                    GtkWidget     *focus,
                                    IdeLayoutGrid *self)
{
  g_assert (IDE_IS_LAYOUT_GRID (self));
  g_assert (!focus || GTK_IS_WIDGET (focus));
  g_assert (GTK_IS_WINDOW (toplevel));

  /*
   * Always remove focus style, but don't necessarily drop our last_focus
   * pointer, since we'll need that to restore things. Style will be
   * reapplied if we found a focus widget.
   */
  if (self->last_focus)
    {
      GtkStyleContext *style_context;

      style_context = gtk_widget_get_style_context (GTK_WIDGET (self->last_focus));
      gtk_style_context_remove_class (style_context, "focus-stack");
    }

  if (focus != NULL)
    {
      GtkWidget *parent = focus;

      while (parent && !IDE_IS_LAYOUT_STACK (parent))
        {
          if (GTK_IS_POPOVER (parent))
            parent = gtk_popover_get_relative_to (GTK_POPOVER (parent));
          else
            parent = gtk_widget_get_parent (parent);
        }

      if (IDE_IS_LAYOUT_STACK (parent))
        ide_layout_grid_set_focus (self, IDE_LAYOUT_STACK (parent));
    }
}

static void
ide_layout_grid_toplevel_is_maximized (GtkWidget     *toplevel,
                                       GParamSpec    *pspec,
                                       IdeLayoutGrid *self)
{
  g_return_if_fail (IDE_IS_LAYOUT_GRID (self));

  ide_layout_grid_make_homogeneous (self);
}

static void
ide_layout_grid_hierarchy_changed (GtkWidget *widget,
                                   GtkWidget *previous_toplevel)
{
  IdeLayoutGrid *self = (IdeLayoutGrid *)widget;
  GtkWidget *toplevel;

  g_return_if_fail (IDE_IS_LAYOUT_GRID (self));

  if (GTK_IS_WINDOW (previous_toplevel))
    {
      g_signal_handlers_disconnect_by_func (previous_toplevel,
                                            G_CALLBACK (ide_layout_grid_toplevel_set_focus),
                                            self);
      g_signal_handlers_disconnect_by_func (previous_toplevel,
                                            G_CALLBACK (ide_layout_grid_toplevel_is_maximized),
                                            self);
    }

  toplevel = gtk_widget_get_toplevel (widget);
  if (GTK_IS_WINDOW (toplevel))
    {
      g_signal_connect (toplevel,
                        "set-focus",
                        G_CALLBACK (ide_layout_grid_toplevel_set_focus),
                        self);
      g_signal_connect (toplevel,
                        "notify::is-maximized",
                        G_CALLBACK (ide_layout_grid_toplevel_is_maximized),
                        self);
    }
}

static void
ide_layout_grid_size_allocate (GtkWidget     *widget,
                               GtkAllocation *alloc)
{
  IdeLayoutGrid *self = (IdeLayoutGrid *)widget;
  GArray *values;
  GtkAllocation prev_alloc;
  GList *stacks;
  GList *iter;
  gsize i;

  g_assert (GTK_IS_WIDGET (widget));

  /*
   * The following code tries to get the width of each stack as a percentage of the
   * view grids width. After size allocate, we attempt to place the position values
   * at the matching percentage. This is needed since we have "recursive panes".
   * A multi-pane would probably make this unnecessary.
   */
  gtk_widget_get_allocation (GTK_WIDGET (self), &prev_alloc);
  values = g_array_new (FALSE, FALSE, sizeof (gdouble));
  stacks = ide_layout_grid_get_stacks (self);

  for (iter = stacks; iter; iter = iter->next)
    {
      GtkWidget *parent;
      guint position;
      gdouble value;

      parent = gtk_widget_get_parent (iter->data);
      position = gtk_paned_get_position (GTK_PANED (parent));
      value = position / (gdouble)prev_alloc.width;
      g_array_append_val (values, value);
    }

  GTK_WIDGET_CLASS (ide_layout_grid_parent_class)->size_allocate (widget, alloc);

  for (iter = stacks, i = 0; iter; iter = iter->next, i++)
    {
      GtkWidget *parent;
      gdouble value;

      parent = gtk_widget_get_parent (iter->data);
      value = g_array_index (values, gdouble, i);
      gtk_paned_set_position (GTK_PANED (parent), value * alloc->width);
    }

  g_array_free (values, TRUE);
  g_list_free (stacks);
}

static void
ide_layout_grid_finalize (GObject *object)
{
  IdeLayoutGrid *self = (IdeLayoutGrid *)object;

  ide_clear_weak_pointer (&self->last_focus);

  G_OBJECT_CLASS (ide_layout_grid_parent_class)->finalize (object);
}

static void
ide_layout_grid_class_init (IdeLayoutGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_layout_grid_finalize;

  widget_class->grab_focus = ide_layout_grid_grab_focus;
  widget_class->hierarchy_changed = ide_layout_grid_hierarchy_changed;
  widget_class->size_allocate = ide_layout_grid_size_allocate;

  gtk_widget_class_set_css_name (widget_class, "layoutgrid");
}

static void
ide_layout_grid_init (IdeLayoutGrid *self)
{
  g_autoptr(GSimpleActionGroup) actions = NULL;
  static const GActionEntry entries[] = {
    { "focus-neighbor", ide_layout_grid_focus_neighbor_action, "i" },
  };
  IdeLayoutStack *stack;
  GtkPaned *paned;

  paned = ide_layout_grid_create_paned (self);
  stack = ide_layout_grid_create_stack (self);

  gtk_container_add_with_properties (GTK_CONTAINER (paned), GTK_WIDGET (stack),
                                     "shrink", FALSE,
                                     "resize", TRUE,
                                     NULL);

  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (paned));

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), entries, G_N_ELEMENTS (entries), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "view-grid", G_ACTION_GROUP (actions));
}

/**
 * ide_layout_grid_get_last_focus:
 * @self: A #IdeLayoutGrid.
 *
 * Gets the last focused #IdeLayoutStack.
 *
 * Returns: (transfer none) (nullable): A #IdeLayoutStack or %NULL.
 */
GtkWidget *
ide_layout_grid_get_last_focus (IdeLayoutGrid *self)
{
  GtkWidget *ret = NULL;
  GList *list;

  g_return_val_if_fail (IDE_IS_LAYOUT_GRID (self), NULL);

  if (self->last_focus != NULL)
    return GTK_WIDGET (self->last_focus);

  list = ide_layout_grid_get_stacks (self);
  ret = list ? list->data : NULL;
  g_list_free (list);

  return ret;
}

/**
 * ide_layout_grid_foreach_view:
 * @self: A #IdeLayoutGrid.
 * @callback: (scope call): A #GtkCallback
 * @user_data: user data for @callback.
 *
 * Calls @callback for every view found in the #IdeLayoutGrid.
 */
void
ide_layout_grid_foreach_view (IdeLayoutGrid *self,
                              GtkCallback    callback,
                              gpointer       user_data)
{
  GList *stacks;
  GList *iter;

  g_return_if_fail (IDE_IS_LAYOUT_GRID (self));
  g_return_if_fail (callback != NULL);

  stacks = ide_layout_grid_get_stacks (self);

  for (iter = stacks; iter; iter = iter->next)
    {
      IdeLayoutStack *stack = iter->data;

      ide_layout_stack_foreach_view (stack, callback, user_data);
    }

  g_list_free (stacks);
}
