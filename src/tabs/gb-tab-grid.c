/* gb-tab-grid.c
 *
 * Copyright (C) 2011 Christian Hergert <chris@dronelabs.com>
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

#define G_LOG_DOMAIN "tab-grid"

#include <glib/gi18n.h>

#include "gb-log.h"
#include "gb-tab.h"
#include "gb-tab-stack.h"
#include "gb-tab-grid.h"

struct _GbTabGridPrivate
{
  GSimpleActionGroup *actions;

  GtkWidget          *top_hpaned;
  GbTabStack         *last_focused_stack;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbTabGrid, gb_tab_grid, GTK_TYPE_BIN)

static GtkWidget *
gb_tab_grid_get_first_stack (GbTabGrid*);

static void
gb_tab_grid_set_last_focused (GbTabGrid  *grid,
                              GbTabStack *stack)
{
  GbTabGridPrivate *priv;

  g_return_if_fail (GB_IS_TAB_GRID (grid));
  g_return_if_fail (!stack || GB_TAB_STACK (stack));

  priv = grid->priv;

  if (priv->last_focused_stack)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->last_focused_stack),
                                    (gpointer *)&priv->last_focused_stack);
      priv->last_focused_stack = NULL;
    }

  if (stack)
    {
      priv->last_focused_stack = stack;
      g_object_add_weak_pointer (G_OBJECT (stack),
                                 (gpointer *)&priv->last_focused_stack);
    }
}

GtkWidget *
gb_tab_grid_new (void)
{
  return g_object_new (GB_TYPE_TAB_GRID, NULL);
}

static void
gb_tab_grid_remove_empty (GbTabGrid *self)
{
  GbTabGridPrivate *priv;
  GtkWidget *paned;
  GtkWidget *stack;
  GtkWidget *parent;
  GtkWidget *child;

  ENTRY;

  g_return_if_fail (GB_IS_TAB_GRID (self));

  priv = self->priv;

  paned = gtk_paned_get_child2 (GTK_PANED (priv->top_hpaned));
  g_assert (GTK_IS_PANED (paned));

  while (paned)
    {
      stack = gtk_paned_get_child1 (GTK_PANED (paned));
      g_assert (GB_IS_TAB_STACK (stack));

      if (!gb_tab_stack_get_n_tabs (GB_TAB_STACK (stack)))
        {
          child = gtk_paned_get_child2 (GTK_PANED (paned));
          g_object_ref (child);
          parent = gtk_widget_get_parent (paned);
          gtk_container_remove (GTK_CONTAINER (paned), child);
          gtk_container_remove (GTK_CONTAINER (parent), paned);
          gtk_paned_add2 (GTK_PANED (parent), child);
          g_object_unref (child);
          paned = parent;
        }

      paned = gtk_paned_get_child2 (GTK_PANED (paned));
    }

  /*
   * If everything got removed, re-add a default stack.
   */
  if (!gtk_paned_get_child2 (GTK_PANED (priv->top_hpaned)))
    (void)gb_tab_grid_get_first_stack (self);

  EXIT;
}

static GtkWidget *
gb_tab_grid_get_first_stack (GbTabGrid *self)
{
  GbTabGridPrivate *priv;
  GtkWidget *child;
  GtkWidget *paned;

  ENTRY;

  g_return_val_if_fail (GB_IS_TAB_GRID (self), NULL);

  priv = self->priv;

  if (!(paned = gtk_paned_get_child2 (GTK_PANED (priv->top_hpaned))))
    {
      paned = g_object_new (GTK_TYPE_PANED,
                            "orientation", GTK_ORIENTATION_HORIZONTAL,
                            "visible", TRUE,
                            NULL);
      gtk_paned_add2 (GTK_PANED (priv->top_hpaned), paned);
      gtk_container_child_set (GTK_CONTAINER (priv->top_hpaned), paned,
                               "resize", TRUE,
                               "shrink", FALSE,
                               NULL);
      child = g_object_new (GB_TYPE_TAB_STACK,
                            "visible", TRUE,
                            NULL);
      g_signal_connect_swapped (child, "changed",
                                G_CALLBACK (gb_tab_grid_remove_empty),
                                self);
      gtk_paned_add1 (GTK_PANED (paned), child);
    }

  child = gtk_paned_get_child1 (GTK_PANED (paned));

  RETURN (child);
}

static GbTabStack *
gb_tab_grid_get_last_focused (GbTabGrid *grid)
{
  g_return_val_if_fail (GB_IS_TAB_GRID (grid), NULL);

  if (!grid->priv->last_focused_stack)
    return (GbTabStack *)gb_tab_grid_get_first_stack (grid);

  return grid->priv->last_focused_stack;
}

GbTab *
gb_tab_grid_get_active (GbTabGrid *grid)
{
  GbTabStack *last_focused_stack;
  GbTab *ret = NULL;

  g_return_val_if_fail (GB_IS_TAB_GRID (grid), NULL);

  last_focused_stack = gb_tab_grid_get_last_focused (grid);

  if (last_focused_stack)
    ret = gb_tab_stack_get_active (last_focused_stack);

  return ret;
}

static void
gb_tab_grid_add (GtkContainer *container,
                 GtkWidget    *child)
{
  GbTabGrid *self = (GbTabGrid *) container;
  GbTabStack *stack = NULL;

  g_return_if_fail (GB_IS_TAB_GRID (self));
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (GB_IS_TAB (child))
    {
      stack = gb_tab_grid_get_last_focused (self);
      gtk_container_add (GTK_CONTAINER (stack), child);
    }
  else
    gtk_paned_add1 (GTK_PANED (self->priv->top_hpaned), child);
}

static GList *
gb_tab_grid_get_stacks (GbTabGrid *self)
{
  GbTabGridPrivate *priv;
  GtkWidget *child;
  GtkWidget *paned;
  GList *list = NULL;

  ENTRY;

  g_return_val_if_fail (GB_IS_TAB_GRID (self), NULL);

  priv = self->priv;

  paned = priv->top_hpaned;

  for (; paned; paned = gtk_paned_get_child2 (GTK_PANED (paned)))
    {
      child = gtk_paned_get_child1 (GTK_PANED (paned));
      if (GB_IS_TAB_STACK (child))
        list = g_list_append (list, child);
    }

  RETURN (list);
}

GList *
gb_tab_grid_get_tabs (GbTabGrid *self)
{
  GList *stacks;
  GList *iter;
  GList *ret = NULL;

  ENTRY;

  g_return_val_if_fail (GB_IS_TAB_GRID (self), NULL);

  stacks = gb_tab_grid_get_stacks (self);
  for (iter = stacks; iter; iter = iter->next)
    ret = g_list_concat (ret, gb_tab_stack_get_tabs (iter->data));
  g_list_free (stacks);

  RETURN (ret);
}

static void
gb_tab_grid_realign (GbTabGrid *self)
{
  GbTabGridPrivate *priv;
  GtkAllocation alloc;
  GtkWidget *paned;
  guint n_paneds = 0;
  guint width;

  ENTRY;

  g_return_if_fail (GB_IS_TAB_GRID (self));

  priv = self->priv;

  paned = gtk_paned_get_child2 (GTK_PANED (priv->top_hpaned));
  do
    n_paneds++;
  while ((paned = gtk_paned_get_child2 (GTK_PANED (paned))));
  g_assert_cmpint (n_paneds, >, 0);

  paned = gtk_paned_get_child2 (GTK_PANED (priv->top_hpaned));
  gtk_widget_get_allocation (paned, &alloc);
  width = alloc.width / n_paneds;

  do
    gtk_paned_set_position (GTK_PANED (paned), width);
  while ((paned = gtk_paned_get_child2 (GTK_PANED (paned))));

  EXIT;
}

static GbTabStack *
gb_tab_grid_prepend_stack (GbTabGrid *self)
{
  GbTabGridPrivate *priv;
  GtkWidget *stack;
  GtkWidget *paned;
  GtkWidget *child2;

  ENTRY;

  g_return_val_if_fail (GB_IS_TAB_GRID (self), NULL);

  priv = self->priv;

  stack = g_object_new (GB_TYPE_TAB_STACK,
                        "visible", TRUE,
                        NULL);
  g_signal_connect_swapped (stack, "changed",
                            G_CALLBACK (gb_tab_grid_remove_empty),
                            self);
  child2 = gtk_paned_get_child2 (GTK_PANED (priv->top_hpaned));
  g_object_ref (child2);
  gtk_container_remove (GTK_CONTAINER (priv->top_hpaned), child2);
  paned = g_object_new (GTK_TYPE_PANED,
                        "orientation", GTK_ORIENTATION_HORIZONTAL,
                        "visible", TRUE,
                        NULL);
  gtk_paned_add1 (GTK_PANED (paned), stack);
  gtk_paned_add2 (GTK_PANED (paned), child2);
  gtk_container_child_set (GTK_CONTAINER (paned), stack,
                           "resize", TRUE,
                           "shrink", FALSE,
                           NULL);
  gtk_container_child_set (GTK_CONTAINER (paned), child2,
                           "resize", TRUE,
                           "shrink", FALSE,
                           NULL);
  g_object_unref (child2);
  gtk_paned_add2 (GTK_PANED (priv->top_hpaned), paned);

  gb_tab_grid_realign (self);

  RETURN (GB_TAB_STACK (stack));
}

static GbTabStack *
gb_tab_grid_add_stack (GbTabGrid *self)
{
  GbTabGridPrivate *priv;
  GtkWidget *stack_paned;
  GtkWidget *stack = NULL;
  GtkWidget *paned;

  ENTRY;

  g_return_val_if_fail (GB_IS_TAB_GRID (self), NULL);

  priv = self->priv;

  stack = g_object_new (GB_TYPE_TAB_STACK,
                        "visible", TRUE,
                        NULL);
  g_signal_connect_swapped (stack, "changed",
                            G_CALLBACK (gb_tab_grid_remove_empty),
                            self);

  paned = priv->top_hpaned;
  while (gtk_paned_get_child2 (GTK_PANED (paned)))
    paned = gtk_paned_get_child2 (GTK_PANED (paned));

  stack_paned = g_object_new (GTK_TYPE_PANED,
                              "orientation", GTK_ORIENTATION_HORIZONTAL,
                              "visible", TRUE,
                              NULL);
  gtk_paned_add1 (GTK_PANED (stack_paned), stack);
  gtk_container_child_set (GTK_CONTAINER (stack_paned), stack,
                           "resize", TRUE,
                           "shrink", FALSE,
                           NULL);

  gtk_paned_add2 (GTK_PANED (paned), stack_paned);
  gtk_container_child_set (GTK_CONTAINER (paned), stack_paned,
                           "resize", TRUE,
                           "shrink", FALSE,
                           NULL);

  gb_tab_grid_realign (self);

  RETURN (GB_TAB_STACK (stack));
}

void
gb_tab_grid_move_tab_right (GbTabGrid *self,
                            GbTab     *tab)
{
  GbTabStack *stack;
  GList *iter;
  GList *stacks;
  GList *children;

  ENTRY;

  g_return_if_fail (GB_IS_TAB_GRID (self));
  g_return_if_fail (GB_IS_TAB (tab));

  stacks = gb_tab_grid_get_stacks (self);

  for (iter = stacks; iter; iter = iter->next)
    {
      if (gb_tab_stack_contains_tab (iter->data, tab))
        {
          /* if we are the last stack and this is the last item,
           * we can short circuit and do nothing.
           */
          if (!iter->next)
            {
              guint length;

              children = gb_tab_stack_get_tabs (iter->data);
              length = g_list_length (children);
              g_list_free (children);

              if (length == 1)
                break;
            }

          g_object_ref (tab);
          gb_tab_stack_remove_tab (iter->data, tab);
          if (!iter->next)
            stack = gb_tab_grid_add_stack (self);
          else
            stack = iter->next->data;
          gtk_container_add (GTK_CONTAINER (stack), GTK_WIDGET (tab));
          gtk_widget_grab_focus (GTK_WIDGET (tab));
          g_object_unref (tab);
          break;
        }
    }

  g_list_free (stacks);

  gb_tab_grid_remove_empty (self);

  EXIT;
}

void
gb_tab_grid_move_tab_left (GbTabGrid *self,
                           GbTab     *tab)
{
  GbTabStack *stack;
  GList *iter;
  GList *stacks;

  ENTRY;

  g_return_if_fail (GB_IS_TAB_GRID (self));
  g_return_if_fail (GB_IS_TAB (tab));

  stacks = gb_tab_grid_get_stacks (self);

  for (iter = stacks; iter; iter = iter->next)
    {
      if (gb_tab_stack_contains_tab (iter->data, tab))
        {
          g_object_ref (tab);
          gb_tab_stack_remove_tab (iter->data, tab);
          if (!iter->prev)
            stack = gb_tab_grid_prepend_stack (self);
          else
            stack = iter->prev->data;
          gtk_container_add (GTK_CONTAINER (stack), GTK_WIDGET (tab));
          gtk_widget_grab_focus (GTK_WIDGET (tab));
          g_object_unref (tab);
          break;
        }
    }

  g_list_free (stacks);

  gb_tab_grid_remove_empty (self);

  EXIT;
}

void
gb_tab_grid_focus_next_view (GbTabGrid *self,
                             GbTab     *tab)
{
  GList *iter;
  GList *stacks;

  ENTRY;

  g_return_if_fail (GB_IS_TAB_GRID (self));
  g_return_if_fail (GB_IS_TAB (tab));

  /* TODO: track focus so we can drop @tab parameter */

  stacks = gb_tab_grid_get_stacks (self);
  for (iter = stacks; iter; iter = iter->next)
    {
      if (gb_tab_stack_contains_tab (iter->data, tab))
        {
          if (!gb_tab_stack_focus_next (iter->data))
            if (iter->next)
              gb_tab_stack_focus_first (iter->next->data);

          break;
        }
    }
  g_list_free (stacks);

  EXIT;
}

void
gb_tab_grid_focus_previous_view (GbTabGrid *self,
                                 GbTab     *view)
{
  GList *iter;
  GList *stacks;

  ENTRY;

  g_return_if_fail (GB_IS_TAB_GRID (self));
  g_return_if_fail (GB_IS_TAB (view));

  /* TODO: track focus so we can drop @tab parameter */

  stacks = gb_tab_grid_get_stacks (self);
  stacks = g_list_reverse (stacks);
  for (iter = stacks; iter; iter = iter->next)
    {
      if (gb_tab_stack_contains_tab (iter->data, view))
        {
          gb_tab_stack_focus_previous (iter->data);
          break;
        }
    }
  g_list_free (stacks);

  EXIT;
}

static void
on_next_tab (GSimpleAction *action,
             GVariant      *parameters,
             gpointer       user_data)
{
  GbTabGrid *self = user_data;
  GbTabStack *last_focused_stack = NULL;

  ENTRY;

  g_return_if_fail (GB_IS_TAB_GRID (self));

  last_focused_stack = gb_tab_grid_get_last_focused (self);
  if (last_focused_stack)
    if (!gb_tab_stack_focus_next (last_focused_stack))
      gb_tab_stack_focus_first  (last_focused_stack);

  EXIT;
}

static void
on_previous_tab (GSimpleAction *action,
                 GVariant      *parameters,
                 gpointer       user_data)
{
  GbTabGrid *self = user_data;
  GbTabStack *last_focused_stack = NULL;

  ENTRY;

  g_return_if_fail (GB_IS_TAB_GRID (self));

  last_focused_stack = gb_tab_grid_get_last_focused (self);
  if (last_focused_stack)
    if (!gb_tab_stack_focus_previous (last_focused_stack))
      gb_tab_stack_focus_last (last_focused_stack);

  EXIT;
}

static void
on_focus_tab_left (GSimpleAction *action,
                   GVariant      *parameters,
                   gpointer       user_data)
{
  GbTabGrid *self = user_data;
  GbTabStack *last_focused_stack = NULL;
  GList *stacks;
  GList *iter;

  ENTRY;

  g_return_if_fail (GB_IS_TAB_GRID (self));

  last_focused_stack = gb_tab_grid_get_last_focused (self);
  stacks = gb_tab_grid_get_stacks (self);

  for (iter = stacks; iter; iter = iter->next)
    {
      if ((iter->data == (void *)last_focused_stack) &&
          iter->prev && iter->prev->data)
        {
          gtk_widget_grab_focus (iter->prev->data);
          break;
        }
    }

  g_list_free (stacks);

  EXIT;
}

static void
on_focus_tab_right (GSimpleAction *action,
                    GVariant      *parameters,
                    gpointer       user_data)
{
  GbTabGrid *self = user_data;
  GbTabStack *last_focused_stack = NULL;
  GList *stacks;
  GList *iter;

  ENTRY;

  g_return_if_fail (GB_IS_TAB_GRID (self));

  last_focused_stack = gb_tab_grid_get_last_focused (self);
  stacks = gb_tab_grid_get_stacks (self);

  for (iter = stacks; iter; iter = iter->next)
    {
      if ((iter->data == (void *)last_focused_stack) &&
          iter->next && iter->next->data)
        {
          gtk_widget_grab_focus (iter->next->data);
          break;
        }
    }

  g_list_free (stacks);

  EXIT;
}

static void
on_move_right (GSimpleAction *action,
               GVariant      *parameters,
               gpointer       user_data)
{
  GbTabGrid *self = user_data;
  GbTabStack *last_focused_stack = NULL;

  ENTRY;

  g_return_if_fail (GB_IS_TAB_GRID (self));

  last_focused_stack = gb_tab_grid_get_last_focused (self);

  if (GB_IS_TAB_STACK (last_focused_stack))
    {
      GbTab *active;

      active = gb_tab_stack_get_active (last_focused_stack);

      if (active)
        gb_tab_grid_move_tab_right (self, active);
    }

  EXIT;
}

static void
on_move_left (GSimpleAction *action,
              GVariant      *parameters,
              gpointer       user_data)
{
  GbTabGrid *self = user_data;
  GbTabStack *last_focused_stack = NULL;

  ENTRY;

  g_return_if_fail (GB_IS_TAB_GRID (self));

  last_focused_stack = gb_tab_grid_get_last_focused (self);

  if (GB_IS_TAB_STACK (last_focused_stack))
    {
      GbTab *active;

      active = gb_tab_stack_get_active (last_focused_stack);

      if (active)
        gb_tab_grid_move_tab_left (self, active);
    }

  EXIT;
}

static void
gb_tab_grid_on_set_focus (GbTabGrid *grid,
                          GtkWidget *widget,
                          GtkWindow *window)
{
  g_return_if_fail (GB_IS_TAB_GRID (grid));
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (widget)
    {
      while (widget && !GB_IS_TAB_STACK (widget))
        widget = gtk_widget_get_parent (widget);

      if (GB_IS_TAB_STACK (widget))
        gb_tab_grid_set_last_focused (grid, GB_TAB_STACK (widget));
    }
}

static void
gb_tab_grid_realize (GtkWidget *widget)
{
  GtkWidget *toplevel;
  GbTabGrid *grid = (GbTabGrid *)widget;

  g_return_if_fail (GB_IS_TAB_GRID (grid));

  GTK_WIDGET_CLASS (gb_tab_grid_parent_class)->realize (widget);

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    {
      /*
       * WORKAROUND:
       *
       * We need to register our actions with the toplevel or they wont be
       * taken into account when activating accelerators. See bugzilla bug
       * 740682 for a patch to Gtk+.
       */
      gtk_widget_insert_action_group (toplevel, "tabs",
                                      G_ACTION_GROUP (grid->priv->actions));

      /*
       * Track focus so we know where we are moving to/from in the stack.
       */
      g_signal_connect_object (toplevel,
                               "set-focus",
                               G_CALLBACK (gb_tab_grid_on_set_focus),
                               widget,
                               G_CONNECT_SWAPPED);
    }
}

void
gb_tab_grid_focus_tab (GbTabGrid *grid,
                       GbTab     *tab)
{
  GList *stacks;
  GList *iter;

  g_return_if_fail (GB_IS_TAB_GRID (grid));
  g_return_if_fail (GB_IS_TAB (tab));

  stacks = gb_tab_grid_get_stacks (grid);

  for (iter = stacks; iter; iter = iter->next)
    {
      if (gb_tab_stack_contains_tab (iter->data, tab))
        {
          gb_tab_stack_focus_tab (iter->data, tab);
          break;
        }
    }
}

GbTab *
gb_tab_grid_find_tab_typed (GbTabGrid *grid,
                            GType      type)
{
  GbTab *ret = NULL;
  GList *list;
  GList *iter;

  g_return_val_if_fail (GB_IS_TAB_GRID (grid), NULL);
  g_return_val_if_fail (g_type_is_a (type, GB_TYPE_TAB), NULL);

  list = gb_tab_grid_get_tabs (grid);

  for (iter = list; iter; iter = iter->next)
    {
      if (g_type_is_a (G_TYPE_FROM_INSTANCE (iter->data), type))
        {
          ret = iter->data;
          break;
        }
    }

  g_list_free (list);

  return ret;
}

static void
gb_tab_grid_grab_focus (GtkWidget *widget)
{
  GbTabGrid *grid = (GbTabGrid *)widget;
  GbTabStack *stack;

  g_return_if_fail (GB_IS_TAB_GRID (grid));

  stack = gb_tab_grid_get_last_focused (grid);

  gtk_widget_grab_focus (GTK_WIDGET (stack));
}

static void
gb_tab_grid_finalize (GObject *object)
{
  GbTabGridPrivate *priv = GB_TAB_GRID (object)->priv;

  g_clear_object (&priv->actions);

  G_OBJECT_CLASS (gb_tab_grid_parent_class)->finalize (object);
}

/**
 * gb_tab_grid_class_init:
 * @klass: (in): A #GbTabGridClass.
 *
 * Initializes the #GbTabGridClass and prepares the vtable.
 */
static void
gb_tab_grid_class_init (GbTabGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->finalize = gb_tab_grid_finalize;

  widget_class->realize = gb_tab_grid_realize;
  widget_class->grab_focus = gb_tab_grid_grab_focus;

  container_class->add = gb_tab_grid_add;
}

/**
 * gb_tab_grid_init:
 * @self: (in): A #GbTabGrid.
 *
 * Initializes the newly created #GbTabGrid instance.
 */
static void
gb_tab_grid_init (GbTabGrid *self)
{
  static const GActionEntry entries[] = {
    { "next", on_next_tab },
    { "previous", on_previous_tab },
    { "right", on_focus_tab_right },
    { "left", on_focus_tab_left },
    { "move-right", on_move_right },
    { "move-left", on_move_left },
  };
  GtkWidget *paned;
  GtkWidget *stack;

  self->priv = gb_tab_grid_get_instance_private (self);

  self->priv->actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->priv->actions),
                                   entries, G_N_ELEMENTS (entries), self);

  self->priv->top_hpaned =
    g_object_new (GTK_TYPE_PANED,
                  "orientation", GTK_ORIENTATION_HORIZONTAL,
                  "visible", TRUE,
                  NULL);
  GTK_CONTAINER_CLASS (gb_tab_grid_parent_class)->add (GTK_CONTAINER (self),
                                                       self->priv->top_hpaned);

  paned = g_object_new (GTK_TYPE_PANED,
                        "orientation", GTK_ORIENTATION_HORIZONTAL,
                        "visible", TRUE,
                        NULL);
  gtk_paned_add2 (GTK_PANED (self->priv->top_hpaned), paned);

  stack = g_object_new (GB_TYPE_TAB_STACK,
                        "visible", TRUE,
                        NULL);
  g_signal_connect_swapped (stack, "changed",
                            G_CALLBACK (gb_tab_grid_remove_empty),
                            self);
  gtk_paned_add1 (GTK_PANED (paned), stack);
  gtk_container_child_set (GTK_CONTAINER (paned), stack,
                           "resize", TRUE,
                           "shrink", FALSE,
                           NULL);
}
