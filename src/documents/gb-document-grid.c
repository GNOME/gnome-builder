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
#include "gb-widget.h"
#include "gb-close-confirmation-dialog.h"

struct _GbDocumentGridPrivate
{
  GbDocumentManager *document_manager;
  GbDocumentStack   *last_focus;
  GtkSizeGroup      *title_size_group;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbDocumentGrid, gb_document_grid, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_DOCUMENT_MANAGER,
  PROP_TITLE_SIZE_GROUP,
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
  GtkWidget *new_focus;
  GList *stacks;
  GList *iter;

  g_return_if_fail (GB_IS_DOCUMENT_GRID (grid));
  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));

  stacks = gb_document_grid_get_stacks (grid);

  /* refuse to remove the stack if there is only one */
  if (g_list_length (stacks) == 1)
    return;

  new_focus = gb_document_grid_get_stack_before (grid, stack);
  if (!new_focus)
    new_focus = gb_document_grid_get_stack_after (grid, stack);

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

  if (new_focus)
    gtk_widget_grab_focus (new_focus);

  g_list_free (stacks);
}

static GtkWidget *
gb_document_grid_get_first_stack (GbDocumentGrid *grid)
{
  GtkWidget *child;

  g_return_val_if_fail (GB_IS_DOCUMENT_GRID (grid), NULL);

  child = gtk_bin_get_child (GTK_BIN (grid));

  if (GTK_IS_PANED (child))
    {
      child = gtk_paned_get_child1 (GTK_PANED (child));
      if (GB_IS_DOCUMENT_STACK (child))
        return child;
    }

  return NULL;
}

static GtkWidget *
gb_document_grid_get_last_stack (GbDocumentGrid *grid)
{
  GtkWidget *child;
  GtkWidget *child2;

  g_return_val_if_fail (GB_IS_DOCUMENT_GRID (grid), NULL);

  child = gtk_bin_get_child (GTK_BIN (grid));

  while (GTK_IS_PANED (child) &&
         (child2 = gtk_paned_get_child2 (GTK_PANED (child))))
    child = child2;

  child = gtk_paned_get_child1 (GTK_PANED (child));

  if (GB_IS_DOCUMENT_STACK (child))
    return child;

  return NULL;
}

static void
gb_document_grid_focus_neighbor (GbDocumentGrid   *grid,
                                 GtkDirectionType  dir,
                                 GbDocumentStack  *stack)
{
  GtkWidget *neighbor;

  g_return_if_fail (GB_IS_DOCUMENT_GRID (grid));
  g_return_if_fail (GB_IS_DOCUMENT_STACK (stack));

  switch ((int)dir)
    {
    case GTK_DIR_LEFT:
      neighbor = gb_document_grid_get_stack_before (grid, stack);
      if (!neighbor)
        neighbor = gb_document_grid_get_last_stack (grid);
      break;

    case GTK_DIR_RIGHT:
      neighbor = gb_document_grid_get_stack_after (grid, stack);
      if (!neighbor)
        neighbor = gb_document_grid_get_first_stack (grid);
      break;

    default:
      neighbor = NULL;
      break;
    }

  if (neighbor != NULL)
    gtk_widget_grab_focus (neighbor);
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

  gb_document_grid_focus_neighbor (grid, GTK_DIR_LEFT, stack);
  gb_document_grid_remove_stack (grid, stack);

cleanup:
  g_list_free (stacks);
}

static gboolean
gb_document_grid_request_close (GbDocumentGrid *grid,
                                GbDocumentView *view)
{
  gboolean ret = FALSE;
  GbDocument *document;

  g_return_val_if_fail (GB_IS_DOCUMENT_GRID (grid), FALSE);
  g_return_val_if_fail (GB_IS_DOCUMENT_VIEW (view), FALSE);

  document = gb_document_view_get_document (view);
  if (!document)
    return FALSE;

  if (gb_document_get_modified (document))
    {
      GbWorkbench *workbench;
      GtkWidget *dialog;
      gint response_id;

      workbench = gb_widget_get_workbench (GTK_WIDGET (view));
      dialog = gb_close_confirmation_dialog_new_single (GTK_WINDOW (workbench),
                                                 document);
      response_id = gtk_dialog_run (GTK_DIALOG (dialog));

      switch (response_id)
        {
        case GTK_RESPONSE_YES:
          if (gb_document_is_untitled (document))
            gb_document_save_as_async (document, GTK_WIDGET (workbench), NULL, NULL, NULL);
          else
            gb_document_save_async (document, GTK_WIDGET (workbench), NULL, NULL, NULL);
          break;

        case GTK_RESPONSE_NO:
          break;

        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
          ret = TRUE;
          break;

        default:
          g_assert_not_reached ();
        }

      gtk_widget_hide (dialog);
      gtk_widget_destroy (dialog);
    }

  return ret;
}

static void
gb_document_grid_view_closed (GbDocumentGrid *grid,
                              GbDocumentView *view)
{
  GbDocument *document;
  GList *stacks;
  GList *iter;

  /*
   * This function will attempt to close the document with the underlying
   * document manager if this was the last open view of the document and the
   * document is not currently modified.
   */

  g_return_if_fail (GB_IS_DOCUMENT_GRID (grid));
  g_return_if_fail (GB_IS_DOCUMENT_VIEW (view));

  document = gb_document_view_get_document (view);
  if (!document)
    return;

  stacks = gb_document_grid_get_stacks (grid);

  for (iter = stacks; iter; iter = iter->next)
    {
      GbDocumentStack *stack = iter->data;

      if (gb_document_stack_find_with_document (stack, document))
        goto cleanup;
    }

  gb_document_manager_remove (grid->priv->document_manager, document);

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

  g_signal_connect_object (stack,
                           "focus-neighbor",
                           G_CALLBACK (gb_document_grid_focus_neighbor),
                           grid,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (stack,
                           "view-closed",
                           G_CALLBACK (gb_document_grid_view_closed),
                           grid,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  g_signal_connect_object (stack,
                           "request-close",
                           G_CALLBACK (gb_document_grid_request_close),
                           grid,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

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
gb_document_grid_close_untitled (GbDocumentGrid *grid)
{
  GList *documents;
  GList *diter;
  GList *stacks;
  GList *siter;

  g_return_if_fail (GB_IS_DOCUMENT_GRID (grid));

  documents = gb_document_manager_get_documents (grid->priv->document_manager);
  stacks = gb_document_grid_get_stacks (grid);

  g_list_foreach (documents, (GFunc)g_object_ref, NULL);
  g_list_foreach (stacks, (GFunc)g_object_ref, NULL);

  for (diter = documents; diter; diter = diter->next)
    {
      if (gb_document_get_modified (diter->data) ||
          !gb_document_is_untitled (diter->data))
        continue;

      for (siter = stacks; siter; siter = siter->next)
        {
          GtkWidget *view;

          view = gb_document_stack_find_with_document (siter->data,
                                                       diter->data);
          if (view)
            gb_document_stack_remove_view (siter->data,
                                           GB_DOCUMENT_VIEW (view));
        }
    }

  g_list_foreach (documents, (GFunc)g_object_unref, NULL);
  g_list_foreach (stacks, (GFunc)g_object_unref, NULL);

  g_list_free (documents);
  g_list_free (stacks);
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
gb_document_grid_grab_focus (GtkWidget *widget)
{
  GbDocumentGrid *grid = (GbDocumentGrid *)widget;
  GList *stacks;

  g_return_if_fail (GB_IS_DOCUMENT_GRID (grid));

  if (grid->priv->last_focus)
    {
      gtk_widget_grab_focus (GTK_WIDGET (grid->priv->last_focus));
      return;
    }

  stacks = gb_document_grid_get_stacks (grid);
  if (stacks)
    gtk_widget_grab_focus (stacks->data);
  g_list_free (stacks);
}

static void
gb_document_grid_toplevel_set_focus (GtkWidget      *toplevel,
                                     GtkWidget      *focus,
                                     GbDocumentGrid *self)
{
  GbDocumentGridPrivate *priv;

  g_return_if_fail (GB_IS_DOCUMENT_GRID (self));

  priv = self->priv;

  if (focus && gtk_widget_is_ancestor (focus, GTK_WIDGET (self)))
    {
      GtkWidget *parent = focus;

      while (parent && !GB_IS_DOCUMENT_STACK (parent))
        parent = gtk_widget_get_parent (parent);

      if (GB_IS_DOCUMENT_STACK (parent))
        {
          if (priv->last_focus)
            {
              g_object_remove_weak_pointer (G_OBJECT (priv->last_focus),
                                            (gpointer *)&priv->last_focus);
              priv->last_focus = NULL;
            }

          priv->last_focus = GB_DOCUMENT_STACK (focus);
          g_object_add_weak_pointer (G_OBJECT (priv->last_focus),
                                     (gpointer *)&priv->last_focus);
        }
    }
}

static void
gb_document_grid_parent_set (GtkWidget      *widget,
                             GtkWidget      *previous_parent)
{
  GbDocumentGrid *self = (GbDocumentGrid *)widget;
  GtkWidget *toplevel;

  g_return_if_fail (GB_IS_DOCUMENT_GRID (self));

  if (previous_parent)
    {
      toplevel = gtk_widget_get_toplevel (previous_parent);
      if (toplevel)
        g_signal_handlers_disconnect_by_func (previous_parent,
                                              G_CALLBACK (gb_document_grid_toplevel_set_focus),
                                              toplevel);
    }

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    {
      g_signal_connect (toplevel,
                        "set-focus",
                        G_CALLBACK (gb_document_grid_toplevel_set_focus),
                        widget);
    }
}

GtkSizeGroup *
gb_document_grid_get_title_size_group (GbDocumentGrid *grid)
{
  g_return_val_if_fail (GB_IS_DOCUMENT_GRID (grid), NULL);

  return grid->priv->title_size_group;
}

void
gb_document_grid_set_title_size_group (GbDocumentGrid *grid,
                                       GtkSizeGroup   *title_size_group)
{
  GbDocumentGridPrivate *priv;

  g_return_if_fail (GB_IS_DOCUMENT_GRID (grid));

  priv = grid->priv;

  if (g_set_object (&priv->title_size_group, title_size_group))
    {
      GList *stacks;
      GList *iter;

      stacks = gb_document_grid_get_stacks (grid);
      for (iter = stacks; iter; iter = iter->next)
        g_object_set (iter->data, "title-size-group", title_size_group, NULL);
      g_list_free (stacks);

      g_object_notify_by_pspec (G_OBJECT (grid),
                                gParamSpecs [PROP_TITLE_SIZE_GROUP]);
    }
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

    case PROP_TITLE_SIZE_GROUP:
      g_value_set_object (value, gb_document_grid_get_title_size_group (self));
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

    case PROP_TITLE_SIZE_GROUP:
      gb_document_grid_set_title_size_group (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_document_grid_finalize (GObject *object)
{
  GbDocumentGridPrivate *priv = GB_DOCUMENT_GRID (object)->priv;

  if (priv->last_focus)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->last_focus),
                                    (gpointer *)&priv->last_focus);
      priv->last_focus = NULL;
    }

  g_clear_object (&priv->document_manager);

  G_OBJECT_CLASS (gb_document_grid_parent_class)->finalize (object);
}

static void
gb_document_grid_class_init (GbDocumentGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_document_grid_finalize;
  object_class->get_property = gb_document_grid_get_property;
  object_class->set_property = gb_document_grid_set_property;

  widget_class->grab_focus = gb_document_grid_grab_focus;
  widget_class->parent_set = gb_document_grid_parent_set;

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

  /**
   * GbDocumentGrid:title-size-group:
   *
   * A #GtkSizeGroup to be applied to document stacks. This can be used to
   * enforce the height across a row of stacks.
   */
  gParamSpecs [PROP_TITLE_SIZE_GROUP] =
    g_param_spec_object ("title-size-group",
                         _("Title Size Group"),
                         _("A size group to apply to stack titles."),
                         GTK_TYPE_SIZE_GROUP,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TITLE_SIZE_GROUP,
                                   gParamSpecs [PROP_TITLE_SIZE_GROUP]);
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
