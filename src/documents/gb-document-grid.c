/* gb-document-grid.c
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

#define G_LOG_DOMAIN "document-manager"

#include <glib/gi18n.h>

#include "gb-document-grid.h"

struct _GbDocumentGridPrivate
{
  GbDocumentManager *document_manager;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbDocumentGrid, gb_document_grid, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_DOCUMENT_MANAGER,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void gb_document_grid_reposition (GbDocumentGrid *grid);

GtkWidget *
gb_document_grid_new (void)
{
  return g_object_new (GB_TYPE_DOCUMENT_GRID, NULL);
}

static void
gb_document_grid_create_view (GbDocumentGrid  *grid,
                              GbDocument      *document,
                              GbDocumentSplit  split,
                              GbDocumentStack *stack)
{
  GtkWidget *target_stack = NULL;

  g_return_if_fail (GB_IS_DOCUMENT_GRID (grid));
  g_return_if_fail (GB_IS_DOCUMENT (document));
  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));

  switch (split)
    {
    case GB_DOCUMENT_SPLIT_LEFT:
      target_stack = gb_document_grid_get_stack_before (grid, stack);
      if (!target_stack)
        target_stack = gb_document_grid_add_stack_before (grid, stack);
      break;

    case GB_DOCUMENT_SPLIT_RIGHT:
      target_stack = gb_document_grid_get_stack_after (grid, stack);
      if (!target_stack)
        target_stack = gb_document_grid_add_stack_after (grid, stack);
      break;

    case GB_DOCUMENT_SPLIT_NONE:
      break;

    default:
      g_return_if_reached ();
    }

  gb_document_stack_focus_document (GB_DOCUMENT_STACK (target_stack), document);
}

static void
gb_document_grid_remove_stack (GbDocumentGrid  *grid,
                               GbDocumentStack *stack)
{
  GList *stacks;
  GList *iter;

  g_return_if_fail (GB_IS_DOCUMENT_GRID (grid));
  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));

  stacks = gb_document_grid_get_stacks (grid);

  /* refuse to remove the stack if there is only one */
  if (g_list_length (stacks) == 1)
    return;

  for (iter = stacks; iter; iter = iter->next)
    {
      GbDocumentStack *item = GB_DOCUMENT_STACK (iter->data);

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
              paned = gtk_bin_get_child (GTK_BIN (grid));
              child2 = gtk_paned_get_child2 (GTK_PANED (paned));
              g_object_ref (child2);
              gtk_container_remove (GTK_CONTAINER (paned), child2);
              gtk_container_remove (GTK_CONTAINER (grid), paned);
              gtk_container_add (GTK_CONTAINER (grid), child2);
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

          gb_document_grid_reposition (grid);

          break;
        }
    }

  g_list_free (stacks);
}

static void
gb_document_grid_stack_empty (GbDocumentGrid  *grid,
                              GbDocumentStack *stack)
{
  GList *stacks;

  g_return_if_fail (GB_IS_DOCUMENT_GRID (grid));
  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));

  stacks = gb_document_grid_get_stacks (grid);

  g_assert (stacks != NULL);

  if (g_list_length (stacks) == 1)
    goto cleanup;

  gb_document_grid_remove_stack (grid, stack);

cleanup:
  g_list_free (stacks);
}

static GtkPaned *
gb_document_grid_create_paned (GbDocumentGrid *grid)
{
  return g_object_new (GTK_TYPE_PANED,
                       "orientation", GTK_ORIENTATION_HORIZONTAL,
                       "visible", TRUE,
                       NULL);
}

static GbDocumentStack *
gb_document_grid_create_stack (GbDocumentGrid *grid)
{
  GbDocumentStack *stack;

  stack = g_object_new (GB_TYPE_DOCUMENT_STACK,
                        "document-manager", grid->priv->document_manager,
                        "visible", TRUE,
                        NULL);

  g_signal_connect_object (stack,
                           "create-view",
                           G_CALLBACK (gb_document_grid_create_view),
                           grid,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (stack,
                           "empty",
                           G_CALLBACK (gb_document_grid_stack_empty),
                           grid,
                           G_CONNECT_SWAPPED);

  return stack;
}

GbDocumentManager *
gb_document_grid_get_document_manager (GbDocumentGrid *grid)
{
  g_return_val_if_fail (GB_IS_DOCUMENT_GRID (grid), NULL);

  return grid->priv->document_manager;
}

void
gb_document_grid_set_document_manager (GbDocumentGrid    *grid,
                                       GbDocumentManager *document_manager)
{
  GbDocumentGridPrivate *priv;

  g_return_if_fail (GB_IS_DOCUMENT_GRID (grid));
  g_return_if_fail (!document_manager ||
                    GB_IS_DOCUMENT_MANAGER (document_manager));

  priv = grid->priv;

  if (priv->document_manager != document_manager)
    {
      g_clear_object (&priv->document_manager);

      if (document_manager)
        {
          GList *list;
          GList *iter;

          priv->document_manager = g_object_ref (document_manager);

          list = gb_document_grid_get_stacks (grid);

          for (iter = list; iter; iter = iter->next)
            gb_document_stack_set_document_manager (iter->data,
                                                    document_manager);
        }

      g_object_notify_by_pspec (G_OBJECT (grid),
                                gParamSpecs [PROP_DOCUMENT_MANAGER]);
    }
}

static void
gb_document_grid_reposition (GbDocumentGrid *grid)
{
  GtkAllocation alloc;
  GtkWidget *paned;
  GtkWidget *stack;
  guint count = 0;
  guint position;

  g_return_if_fail (GB_IS_DOCUMENT_GRID (grid));

  gtk_widget_get_allocation (GTK_WIDGET (grid), &alloc);

  paned = gtk_bin_get_child (GTK_BIN (grid));

  if (!GTK_IS_PANED (paned))
    return;

  stack = gtk_paned_get_child1 (GTK_PANED (paned));
  do
    {
      count++;
      stack = gb_document_grid_get_stack_after (grid,
                                                GB_DOCUMENT_STACK (stack));
    }
  while (stack);

  position = alloc.width / count;

  stack = gtk_paned_get_child1 (GTK_PANED (paned));
  do
    {
      paned = gtk_widget_get_parent (stack);
      gtk_paned_set_position (GTK_PANED (paned), position);
      stack = gb_document_grid_get_stack_after (grid,
                                                GB_DOCUMENT_STACK (stack));
    }
  while (stack);
}

/**
 * gb_document_grid_get_stacks:
 *
 * Fetches all of the stacks in the grid. The resulting #GList should be
 * freed with g_list_free().
 *
 * Returns: (transfer container) (element-type GbDocumentStack*): A #GList.
 */
GList *
gb_document_grid_get_stacks (GbDocumentGrid *grid)
{
  GtkWidget *paned;
  GList *list = NULL;

  g_return_val_if_fail (GB_IS_DOCUMENT_GRID (grid), NULL);

  paned = gtk_bin_get_child (GTK_BIN (grid));

  while (paned)
    {
      GtkWidget *stack;

      stack = gtk_paned_get_child1 (GTK_PANED (paned));

      if (GB_IS_DOCUMENT_STACK (stack))
        list = g_list_append (list, stack);

      paned = gtk_paned_get_child2 (GTK_PANED (paned));
    }

  return list;
}

GtkWidget *
gb_document_grid_add_stack_before (GbDocumentGrid  *grid,
                                   GbDocumentStack *stack)
{
  GbDocumentStack *new_stack;
  GtkWidget *parent;
  GtkWidget *grandparent;
  GtkPaned *new_paned;

  g_return_val_if_fail (GB_IS_DOCUMENT_GRID (grid), NULL);

  new_paned = gb_document_grid_create_paned (grid);
  new_stack = gb_document_grid_create_stack (grid);
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
  else if (GB_IS_DOCUMENT_GRID (grandparent))
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

  gb_document_grid_reposition (grid);

  return GTK_WIDGET (new_stack);
}

GtkWidget *
gb_document_grid_add_stack_after  (GbDocumentGrid  *grid,
                                   GbDocumentStack *stack)
{
  GbDocumentStack *new_stack;
  GtkWidget *parent;
  GtkPaned *new_paned;

  g_return_val_if_fail (GB_IS_DOCUMENT_GRID (grid), NULL);

  new_paned = gb_document_grid_create_paned (grid);
  new_stack = gb_document_grid_create_stack (grid);
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

  gb_document_grid_reposition (grid);

  return GTK_WIDGET (new_stack);
}

GtkWidget *
gb_document_grid_get_stack_before (GbDocumentGrid  *grid,
                                   GbDocumentStack *stack)
{
  GtkWidget *parent;

  g_return_val_if_fail (GB_IS_DOCUMENT_GRID (grid), NULL);
  g_return_val_if_fail (GB_IS_DOCUMENT_STACK (stack), NULL);

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
gb_document_grid_get_stack_after (GbDocumentGrid  *grid,
                                  GbDocumentStack *stack)
{
  GtkWidget *parent;

  g_return_val_if_fail (GB_IS_DOCUMENT_GRID (grid), NULL);
  g_return_val_if_fail (GB_IS_DOCUMENT_STACK (stack), NULL);

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
gb_document_grid_focus_document (GbDocumentGrid *grid,
                                 GbDocument     *document)
{
  GList *stacks;
  GList *iter;

  g_return_if_fail (GB_IS_DOCUMENT_GRID (grid));
  g_return_if_fail (GB_IS_DOCUMENT (document));

  stacks = gb_document_grid_get_stacks (grid);

  for (iter = stacks; iter; iter = iter->next)
    {
      GbDocumentStack *stack = iter->data;
      GtkWidget *view;

      view = gb_document_stack_find_with_document (stack, document);

      if (view)
        {
          gb_document_stack_focus_document (stack, document);
          goto cleanup;
        }
    }

  g_assert (stacks);

  gb_document_stack_focus_document (stacks->data, document);

cleanup:
  g_list_free (stacks);
}

static void
gb_document_grid_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbDocumentGrid *self = GB_DOCUMENT_GRID(object);

  switch (prop_id)
    {
    case PROP_DOCUMENT_MANAGER:
      g_value_set_object (value, gb_document_grid_get_document_manager (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_document_grid_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GbDocumentGrid *self = GB_DOCUMENT_GRID(object);

  switch (prop_id)
    {
    case PROP_DOCUMENT_MANAGER:
      gb_document_grid_set_document_manager (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_document_grid_finalize (GObject *object)
{
  GbDocumentGridPrivate *priv = GB_DOCUMENT_GRID (object)->priv;

  g_clear_object (&priv->document_manager);

  G_OBJECT_CLASS (gb_document_grid_parent_class)->finalize (object);
}

static void
gb_document_grid_class_init (GbDocumentGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_document_grid_finalize;
  object_class->get_property = gb_document_grid_get_property;
  object_class->set_property = gb_document_grid_set_property;

  /**
   * GbDocumentGrid:document-manager:
   *
   * The "document-manager" property contains the manager for all open
   * "buffers" (known as #GbDocument within Builder).
   */
  gParamSpecs [PROP_DOCUMENT_MANAGER] =
    g_param_spec_object ("document-manager",
                         _("Document Manager"),
                         _("The document manager for the document grid."),
                         GB_TYPE_DOCUMENT_MANAGER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DOCUMENT_MANAGER,
                                   gParamSpecs [PROP_DOCUMENT_MANAGER]);
}

static void
gb_document_grid_init (GbDocumentGrid *self)
{
  GbDocumentStack *stack;
  GtkPaned *paned;

  self->priv = gb_document_grid_get_instance_private (self);

  paned = gb_document_grid_create_paned (self);
  stack = gb_document_grid_create_stack (self);

  gtk_container_add_with_properties (GTK_CONTAINER (paned), GTK_WIDGET (stack),
                                     "shrink", FALSE,
                                     "resize", TRUE,
                                     NULL);

  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (paned));
}
