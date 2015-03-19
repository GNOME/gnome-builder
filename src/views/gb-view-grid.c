/* gb-view-grid.c
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

#define G_LOG_DOMAIN "gb-view-grid"

#include <glib/gi18n.h>

#include "gb-view.h"
#include "gb-view-grid.h"
#include "gb-widget.h"

struct _GbViewGrid
{
  GtkBin       parent_instance;
  GbViewStack *last_focus;
};

G_DEFINE_TYPE (GbViewGrid, gb_view_grid, GTK_TYPE_BIN)

static void gb_view_grid_reposition (GbViewGrid *self);

GtkWidget *
gb_view_grid_new (void)
{
  return g_object_new (GB_TYPE_VIEW_GRID, NULL);
}

static void
gb_view_grid_remove_stack (GbViewGrid  *self,
                           GbViewStack *stack)
{
  GtkWidget *new_focus;
  GList *stacks;
  GList *iter;

  g_return_if_fail (GB_IS_VIEW_GRID (self));
  g_return_if_fail (GB_IS_VIEW_STACK (stack));

  stacks = gb_view_grid_get_stacks (self);

  /* refuse to remove the stack if there is only one */
  if (g_list_length (stacks) == 1)
    return;

  new_focus = gb_view_grid_get_stack_before (self, stack);
  if (!new_focus)
    new_focus = gb_view_grid_get_stack_after (self, stack);

  for (iter = stacks; iter; iter = iter->next)
    {
      GbViewStack *item = GB_VIEW_STACK (iter->data);

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

          gb_view_grid_reposition (self);

          break;
        }
    }

  if (new_focus)
    gtk_widget_grab_focus (new_focus);

  g_list_free (stacks);
}

static GtkWidget *
gb_view_grid_get_first_stack (GbViewGrid *self)
{
  GtkWidget *child;

  g_return_val_if_fail (GB_IS_VIEW_GRID (self), NULL);

  child = gtk_bin_get_child (GTK_BIN (self));

  if (GTK_IS_PANED (child))
    {
      child = gtk_paned_get_child1 (GTK_PANED (child));
      if (GB_IS_VIEW_STACK (child))
        return child;
    }

  return NULL;
}

static GtkWidget *
gb_view_grid_get_last_stack (GbViewGrid *self)
{
  GtkWidget *child;
  GtkWidget *child2;

  g_return_val_if_fail (GB_IS_VIEW_GRID (self), NULL);

  child = gtk_bin_get_child (GTK_BIN (self));

  while (GTK_IS_PANED (child) &&
         (child2 = gtk_paned_get_child2 (GTK_PANED (child))))
    child = child2;

  child = gtk_paned_get_child1 (GTK_PANED (child));

  if (GB_IS_VIEW_STACK (child))
    return child;

  return NULL;
}

static void
gb_view_grid_focus_neighbor (GbViewGrid       *self,
                             GtkDirectionType  dir,
                             GbViewStack      *stack)
{
  GtkWidget *neighbor;

  g_return_if_fail (GB_IS_VIEW_GRID (self));
  g_return_if_fail (GB_IS_VIEW_STACK (stack));

  switch ((int)dir)
    {
    case GTK_DIR_LEFT:
      neighbor = gb_view_grid_get_stack_before (self, stack);
      if (!neighbor)
        neighbor = gb_view_grid_get_last_stack (self);
      break;

    case GTK_DIR_RIGHT:
      neighbor = gb_view_grid_get_stack_after (self, stack);
      if (!neighbor)
        neighbor = gb_view_grid_get_first_stack (self);
      break;

    default:
      neighbor = NULL;
      break;
    }

  if (neighbor != NULL)
    gtk_widget_grab_focus (neighbor);
}

static void
gb_view_grid_stack_empty (GbViewGrid  *self,
                          GbViewStack *stack)
{
  GList *stacks;

  g_return_if_fail (GB_IS_VIEW_GRID (self));
  g_return_if_fail (GB_IS_VIEW_STACK (stack));

  stacks = gb_view_grid_get_stacks (self);

  g_assert (stacks != NULL);

  if (g_list_length (stacks) == 1)
    goto cleanup;

  gb_view_grid_focus_neighbor (self, GTK_DIR_LEFT, stack);
  gb_view_grid_remove_stack (self, stack);

cleanup:
  g_list_free (stacks);
}

static void
gb_view_grid_stack_split (GbViewGrid      *self,
                          GbView          *view,
                          GbViewGridSplit  split,
                          GbViewStack     *stack)
{
  GbDocument *document;
  GtkWidget *target;
  GtkWidget *new_view;

  g_assert (GB_IS_VIEW (view));
  g_assert (GB_IS_VIEW_GRID (self));
  g_assert (GB_IS_VIEW_STACK (stack));

  document = gb_view_get_document (view);
  if (document == NULL)
    return;

  switch (split)
    {
    case GB_VIEW_GRID_SPLIT_LEFT:
      target = gb_view_grid_get_stack_before (self, stack);
      if (target == NULL)
        target = gb_view_grid_add_stack_before (self, stack);
      new_view = gb_document_create_view (document);
      if (new_view == NULL)
        return;
      gb_view_stack_focus_document (GB_VIEW_STACK (target), document);
      break;

    case GB_VIEW_GRID_MOVE_LEFT:
      target = gb_view_grid_get_stack_before (self, stack);
      if (target == NULL)
        target = gb_view_grid_add_stack_before (self, stack);
      new_view = gb_document_create_view (document);
      if (new_view == NULL)
        return;
      gb_view_stack_remove (stack, view);
      gb_view_stack_focus_document (GB_VIEW_STACK (target), document);
      break;

    case GB_VIEW_GRID_SPLIT_RIGHT:
      target = gb_view_grid_get_stack_after (self, stack);
      if (target == NULL)
        target = gb_view_grid_add_stack_after (self, stack);
      new_view = gb_document_create_view (document);
      if (new_view == NULL)
        return;
      gb_view_stack_focus_document (GB_VIEW_STACK (target), document);
      break;

    case GB_VIEW_GRID_MOVE_RIGHT:
      target = gb_view_grid_get_stack_after (self, stack);
      if (target == NULL)
        target = gb_view_grid_add_stack_after (self, stack);
      new_view = gb_document_create_view (document);
      if (new_view == NULL)
        return;
      gb_view_stack_remove (stack, view);
      gb_view_stack_focus_document (GB_VIEW_STACK (target), document);
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static GtkPaned *
gb_view_grid_create_paned (GbViewGrid *self)
{
  return g_object_new (GTK_TYPE_PANED,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       "visible", TRUE,
                       NULL);
}

static GbViewStack *
gb_view_grid_create_stack (GbViewGrid *self)
{
  GbViewStack *stack;

  g_assert (GB_IS_VIEW_GRID (self));

  stack = g_object_new (GB_TYPE_VIEW_STACK,
                        "visible", TRUE,
                        NULL);

  g_signal_connect_object (stack,
                           "empty",
                           G_CALLBACK (gb_view_grid_stack_empty),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (stack,
                           "split",
                           G_CALLBACK (gb_view_grid_stack_split),
                           self,
                           G_CONNECT_SWAPPED);

  return stack;
}

static void
gb_view_grid_reposition (GbViewGrid *self)
{
  GtkAllocation alloc;
  GtkWidget *paned;
  GtkWidget *stack;
  guint count = 0;
  guint position;

  g_return_if_fail (GB_IS_VIEW_GRID (self));

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  paned = gtk_bin_get_child (GTK_BIN (self));

  if (!GTK_IS_PANED (paned))
    return;

  stack = gtk_paned_get_child1 (GTK_PANED (paned));
  g_assert (GB_IS_VIEW_STACK (stack));

  do
    {
      count++;
      stack = gb_view_grid_get_stack_after (self, GB_VIEW_STACK (stack));
      g_assert (!stack || GB_IS_VIEW_STACK (stack));
    }
  while (stack);

  position = alloc.width / count;

  stack = gtk_paned_get_child1 (GTK_PANED (paned));
  g_assert (GB_IS_VIEW_STACK (stack));
  do
    {
      paned = gtk_widget_get_parent (stack);
      gtk_paned_set_position (GTK_PANED (paned), position);
      stack = gb_view_grid_get_stack_after (self, GB_VIEW_STACK (stack));
      g_assert (!stack|| GB_IS_VIEW_STACK (stack));
    }
  while (stack);
}

/**
 * gb_view_grid_get_stacks:
 *
 * Fetches all of the stacks in the grid. The resulting #GList should be
 * freed with g_list_free().
 *
 * Returns: (transfer container) (element-type GbViewStack*): A #GList.
 */
GList *
gb_view_grid_get_stacks (GbViewGrid *self)
{
  GtkWidget *paned;
  GList *list = NULL;

  g_return_val_if_fail (GB_IS_VIEW_GRID (self), NULL);

  paned = gtk_bin_get_child (GTK_BIN (self));

  while (paned)
    {
      GtkWidget *stack;

      stack = gtk_paned_get_child1 (GTK_PANED (paned));

      if (GB_IS_VIEW_STACK (stack))
        list = g_list_append (list, stack);

      paned = gtk_paned_get_child2 (GTK_PANED (paned));
    }

#ifndef IDE_DISABLE_TRACE
  {
    GList *iter;

    for (iter = list; iter; iter = iter->next)
      g_assert (GB_IS_VIEW_STACK (iter->data));
  }
#endif

  return list;
}

GtkWidget *
gb_view_grid_add_stack_before (GbViewGrid  *self,
                               GbViewStack *stack)
{
  GbViewStack *new_stack;
  GtkWidget *parent;
  GtkWidget *grandparent;
  GtkPaned *new_paned;

  g_return_val_if_fail (GB_IS_VIEW_GRID (self), NULL);

  new_paned = gb_view_grid_create_paned (self);
  new_stack = gb_view_grid_create_stack (self);
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
  else if (GB_IS_VIEW_GRID (grandparent))
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

  gb_view_grid_reposition (self);

  return GTK_WIDGET (new_stack);
}

GtkWidget *
gb_view_grid_add_stack_after (GbViewGrid  *self,
                              GbViewStack *stack)
{
  GbViewStack *new_stack;
  GtkWidget *parent;
  GtkPaned *new_paned;

  g_return_val_if_fail (GB_IS_VIEW_GRID (self), NULL);

  new_paned = gb_view_grid_create_paned (self);
  new_stack = gb_view_grid_create_stack (self);
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

  gb_view_grid_reposition (self);

  return GTK_WIDGET (new_stack);
}

GtkWidget *
gb_view_grid_get_stack_before (GbViewGrid  *self,
                               GbViewStack *stack)
{
  GtkWidget *parent;

  g_return_val_if_fail (GB_IS_VIEW_GRID (self), NULL);
  g_return_val_if_fail (GB_IS_VIEW_STACK (stack), NULL);

  parent = gtk_widget_get_parent (GTK_WIDGET (stack));

  if (GTK_IS_PANED (parent))
    {
      parent = gtk_widget_get_parent (parent);
      if (GTK_IS_PANED (parent))
        return gtk_paned_get_child1 (GTK_PANED (parent));
    }

  return NULL;
}

GtkWidget *
gb_view_grid_get_stack_after (GbViewGrid  *self,
                              GbViewStack *stack)
{
  GtkWidget *parent;

  g_return_val_if_fail (GB_IS_VIEW_GRID (self), NULL);
  g_return_val_if_fail (GB_IS_VIEW_STACK (stack), NULL);

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

void
gb_view_grid_focus_document (GbViewGrid *self,
                             GbDocument *document)
{
  GList *stacks;
  GList *iter;

  g_return_if_fail (GB_IS_VIEW_GRID (self));
  g_return_if_fail (GB_IS_DOCUMENT (document));

  stacks = gb_view_grid_get_stacks (self);

  for (iter = stacks; iter; iter = iter->next)
    {
      GbViewStack *stack = iter->data;
      GtkWidget *view;

      view = gb_view_stack_find_with_document (stack, document);

      if (view)
        {
          gb_view_stack_focus_document (stack, document);
          goto cleanup;
        }
    }

  g_assert (stacks);

  if (self->last_focus)
    gb_view_stack_focus_document (self->last_focus, document);
  else
    gb_view_stack_focus_document (stacks->data, document);

cleanup:
  g_list_free (stacks);
}

static void
gb_view_grid_grab_focus (GtkWidget *widget)
{
  GbViewGrid *self = (GbViewGrid *)widget;
  GList *stacks;

  g_return_if_fail (GB_IS_VIEW_GRID (self));

  if (self->last_focus)
    {
      gtk_widget_grab_focus (GTK_WIDGET (self->last_focus));
      return;
    }

  stacks = gb_view_grid_get_stacks (self);
  if (stacks)
    gtk_widget_grab_focus (stacks->data);
  g_list_free (stacks);
}

static void
gb_view_grid_toplevel_set_focus (GtkWidget  *toplevel,
                                 GtkWidget  *focus,
                                 GbViewGrid *self)
{
  g_return_if_fail (GB_IS_VIEW_GRID (self));

  if (focus && gtk_widget_is_ancestor (focus, GTK_WIDGET (self)))
    {
      GtkWidget *parent = focus;

      while (parent && !GB_IS_VIEW_STACK (parent))
        parent = gtk_widget_get_parent (parent);

      if (GB_IS_VIEW_STACK (parent))
        ide_set_weak_pointer (&self->last_focus, GB_VIEW_STACK (parent));
    }
}

static void
gb_view_grid_hierarchy_changed (GtkWidget *widget,
                                GtkWidget *previous_toplevel)
{
  GbViewGrid *self = (GbViewGrid *)widget;
  GtkWidget *toplevel;

  g_return_if_fail (GB_IS_VIEW_GRID (self));

  if (GTK_IS_WINDOW (previous_toplevel))
    g_signal_handlers_disconnect_by_func (previous_toplevel,
                                          G_CALLBACK (gb_view_grid_toplevel_set_focus),
                                          self);

  toplevel = gtk_widget_get_toplevel (widget);
  if (GTK_IS_WINDOW (toplevel))
    g_signal_connect (toplevel,
                      "set-focus",
                      G_CALLBACK (gb_view_grid_toplevel_set_focus),
                      self);
}

static void
gb_view_grid_finalize (GObject *object)
{
  GbViewGrid *self = (GbViewGrid *)object;

  ide_clear_weak_pointer (&self->last_focus);

  G_OBJECT_CLASS (gb_view_grid_parent_class)->finalize (object);
}

static void
gb_view_grid_class_init (GbViewGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_view_grid_finalize;

  widget_class->grab_focus = gb_view_grid_grab_focus;
  widget_class->hierarchy_changed = gb_view_grid_hierarchy_changed;
}

static void
gb_view_grid_init (GbViewGrid *self)
{
  GbViewStack *stack;
  GtkPaned *paned;

  paned = gb_view_grid_create_paned (self);
  stack = gb_view_grid_create_stack (self);

  gtk_container_add_with_properties (GTK_CONTAINER (paned), GTK_WIDGET (stack),
                                     "shrink", FALSE,
                                     "resize", TRUE,
                                     NULL);

  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (paned));
}

GType
gb_view_grid_split_get_type (void)
{
  static gsize type_id;

  if (g_once_init_enter (&type_id))
    {
      static const GEnumValue values[] = {
        { GB_VIEW_GRID_SPLIT_LEFT, "GB_VIEW_GRID_SPLIT_LEFT", "split-left" },
        { GB_VIEW_GRID_SPLIT_RIGHT, "GB_VIEW_GRID_SPLIT_RIGHT", "split-right" },
        { GB_VIEW_GRID_MOVE_LEFT, "GB_VIEW_GRID_MOVE_LEFT", "move-left" },
        { GB_VIEW_GRID_MOVE_RIGHT, "GB_VIEW_GRID_MOVE_RIGHT", "move-right" },
      };
      gsize _type_id;

      _type_id = g_enum_register_static ("GbViewGridSplit", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
