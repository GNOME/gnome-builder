/* pnl-dock-item.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include "pnl-dock-item.h"
#include "pnl-dock-manager.h"
#include "pnl-dock-widget.h"

G_DEFINE_INTERFACE (PnlDockItem, pnl_dock_item, GTK_TYPE_WIDGET)

/*
 * The PnlDockItem is an interface that acts more like a mixin.
 * This is mostly out of wanting to preserve widget inheritance
 * without having to duplicate all sorts of plumbing.
 */

enum {
  MANAGER_SET,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
pnl_dock_item_real_set_manager (PnlDockItem    *self,
                                PnlDockManager *manager)
{
  PnlDockManager *old_manager;

  g_assert (PNL_IS_DOCK_ITEM (self));
  g_assert (!manager || PNL_IS_DOCK_MANAGER (manager));

  if (NULL != (old_manager = pnl_dock_item_get_manager (self)))
    {
      if (PNL_IS_DOCK (self))
        pnl_dock_manager_unregister_dock (old_manager, PNL_DOCK (self));
    }

  if (manager != NULL)
    {
      g_object_set_data_full (G_OBJECT (self),
                              "PNL_DOCK_MANAGER",
                              g_object_ref (manager),
                              g_object_unref);
      if (PNL_IS_DOCK (self))
        pnl_dock_manager_register_dock (manager, PNL_DOCK (self));
    }
  else
    g_object_set_data (G_OBJECT (self), "PNL_DOCK_MANAGER", NULL);

  g_signal_emit (self, signals [MANAGER_SET], 0, old_manager);
}

static PnlDockManager *
pnl_dock_item_real_get_manager (PnlDockItem *self)
{
  g_assert (PNL_IS_DOCK_ITEM (self));

  return g_object_get_data (G_OBJECT (self), "PNL_DOCK_MANAGER");
}

static void
pnl_dock_item_real_update_visibility (PnlDockItem *self)
{
  GtkWidget *parent;

  g_assert (PNL_IS_DOCK_ITEM (self));

  for (parent = gtk_widget_get_parent (GTK_WIDGET (self));
       parent != NULL;
       parent = gtk_widget_get_parent (parent))
    {
      if (PNL_IS_DOCK_ITEM (parent))
        {
          pnl_dock_item_update_visibility (PNL_DOCK_ITEM (parent));
          break;
        }
    }
}

static void
pnl_dock_item_propagate_manager (PnlDockItem *self)
{
  PnlDockManager *manager;
  GPtrArray *ar;
  guint i;

  g_return_if_fail (PNL_IS_DOCK_ITEM (self));

  if (!GTK_IS_CONTAINER (self))
    return;

  if (NULL == (manager = pnl_dock_item_get_manager (self)))
    return;

  if (NULL == (ar = g_object_get_data (G_OBJECT (self), "PNL_DOCK_ITEM_DESCENDANTS")))
    return;

  for (i = 0; i < ar->len; i++)
    {
      PnlDockItem *item = g_ptr_array_index (ar, i);

      pnl_dock_item_set_manager (item, manager);
    }
}

static void
pnl_dock_item_real_manager_set (PnlDockItem    *self,
                                PnlDockManager *manager)
{
  g_assert (PNL_IS_DOCK_ITEM (self));
  g_assert (!manager || PNL_IS_DOCK_MANAGER (manager));

  pnl_dock_item_propagate_manager (self);
}

static void
pnl_dock_item_default_init (PnlDockItemInterface *iface)
{
  iface->get_manager = pnl_dock_item_real_get_manager;
  iface->set_manager = pnl_dock_item_real_set_manager;
  iface->manager_set = pnl_dock_item_real_manager_set;
  iface->update_visibility = pnl_dock_item_real_update_visibility;

  signals [MANAGER_SET] =
    g_signal_new ("manager-set",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (PnlDockItemInterface, manager_set),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, PNL_TYPE_DOCK_MANAGER);
}

/**
 * pnl_dock_item_get_manager:
 * @self: A #PnlDockItem
 *
 * Gets the dock manager for this dock item.
 *
 * Returns: (nullable) (transfer none): A #PnlDockmanager.
 */
PnlDockManager *
pnl_dock_item_get_manager (PnlDockItem *self)
{
  g_return_val_if_fail (PNL_IS_DOCK_ITEM (self), NULL);

  return PNL_DOCK_ITEM_GET_IFACE (self)->get_manager (self);
}

/**
 * pnl_dock_item_set_manager:
 * @self: A #PnlDockItem
 * @manager: (nullable): A #PnlDockManager
 *
 * Sets the dock manager for this #PnlDockItem.
 */
void
pnl_dock_item_set_manager (PnlDockItem    *self,
                           PnlDockManager *manager)
{
  g_return_if_fail (PNL_IS_DOCK_ITEM (self));
  g_return_if_fail (!manager || PNL_IS_DOCK_MANAGER (manager));

  PNL_DOCK_ITEM_GET_IFACE (self)->set_manager (self, manager);
}

void
pnl_dock_item_update_visibility (PnlDockItem *self)
{
  g_return_if_fail (PNL_IS_DOCK_ITEM (self));

  PNL_DOCK_ITEM_GET_IFACE (self)->update_visibility (self);
}

static void
pnl_dock_item_child_weak_notify (gpointer  data,
                                 GObject  *where_object_was)
{
  PnlDockItem *self = data;
  GPtrArray *descendants;

  g_assert (PNL_IS_DOCK_ITEM (self));

  descendants = g_object_get_data (G_OBJECT (self), "PNL_DOCK_ITEM_DESCENDANTS");

  if (descendants != NULL)
    g_ptr_array_remove (descendants, where_object_was);

  pnl_dock_item_update_visibility (self);
}

static void
pnl_dock_item_destroy (PnlDockItem *self)
{
  GPtrArray *descendants;
  guint i;

  g_assert (PNL_IS_DOCK_ITEM (self));

  descendants = g_object_get_data (G_OBJECT (self), "PNL_DOCK_ITEM_DESCENDANTS");

  if (descendants != NULL)
    {
      for (i = 0; i < descendants->len; i++)
        {
          PnlDockItem *child = g_ptr_array_index (descendants, i);

          g_object_weak_unref (G_OBJECT (child),
                               pnl_dock_item_child_weak_notify,
                               self);
        }

      g_object_set_data (G_OBJECT (self), "PNL_DOCK_ITEM_DESCENDANTS", NULL);
      g_ptr_array_unref (descendants);
    }
}

static void
pnl_dock_item_track_child (PnlDockItem *self,
                           PnlDockItem *child)
{
  GPtrArray *descendants;
  guint i;

  g_assert (PNL_IS_DOCK_ITEM (self));
  g_assert (PNL_IS_DOCK_ITEM (child));

  descendants = g_object_get_data (G_OBJECT (self), "PNL_DOCK_ITEM_DESCENDANTS");

  if (descendants == NULL)
    {
      descendants = g_ptr_array_new ();
      g_object_set_data (G_OBJECT (self), "PNL_DOCK_ITEM_DESCENDANTS", descendants);
      g_signal_connect (self,
                        "destroy",
                        G_CALLBACK (pnl_dock_item_destroy),
                        NULL);
    }

  for (i = 0; i < descendants->len; i++)
    {
      PnlDockItem *item = g_ptr_array_index (descendants, i);

      if (item == child)
        return;
    }

  g_object_weak_ref (G_OBJECT (child),
                     pnl_dock_item_child_weak_notify,
                     self);

  g_ptr_array_add (descendants, child);

  pnl_dock_item_update_visibility (child);
}

gboolean
pnl_dock_item_adopt (PnlDockItem *self,
                     PnlDockItem *child)
{
  PnlDockManager *manager;
  PnlDockManager *child_manager;

  g_return_val_if_fail (PNL_IS_DOCK_ITEM (self), FALSE);
  g_return_val_if_fail (PNL_IS_DOCK_ITEM (child), FALSE);

  manager = pnl_dock_item_get_manager (self);
  child_manager = pnl_dock_item_get_manager (child);

  if ((child_manager != NULL) && (manager != NULL) && (child_manager != manager))
    return FALSE;

  if (manager != NULL)
    pnl_dock_item_set_manager (child, manager);

  pnl_dock_item_track_child (self, child);

  return TRUE;
}

void
pnl_dock_item_present_child (PnlDockItem *self,
                             PnlDockItem *child)
{
  g_assert (PNL_IS_DOCK_ITEM (self));
  g_assert (PNL_IS_DOCK_ITEM (child));

#if 0
  g_print ("present_child (%s, %s)\n",
           G_OBJECT_TYPE_NAME (self),
           G_OBJECT_TYPE_NAME (child));
#endif

  if (PNL_DOCK_ITEM_GET_IFACE (self)->present_child)
    PNL_DOCK_ITEM_GET_IFACE (self)->present_child (self, child);
}

/**
 * pnl_dock_item_present:
 * @self: A #PnlDockItem
 *
 * This widget will walk the widget hierarchy to ensure that the
 * dock item is visible to the user.
 */
void
pnl_dock_item_present (PnlDockItem *self)
{
  GtkWidget *parent;

  g_return_if_fail (PNL_IS_DOCK_ITEM (self));

  for (parent = gtk_widget_get_parent (GTK_WIDGET (self));
       parent != NULL;
       parent = gtk_widget_get_parent (parent))
    {
      if (PNL_IS_DOCK_ITEM (parent))
        {
          pnl_dock_item_present_child (PNL_DOCK_ITEM (parent), self);
          pnl_dock_item_present (PNL_DOCK_ITEM (parent));
          return;
        }
    }
}

gboolean
pnl_dock_item_has_widgets (PnlDockItem *self)
{
  GPtrArray *ar;

  g_return_val_if_fail (PNL_IS_DOCK_ITEM (self), FALSE);

  if (PNL_IS_DOCK_WIDGET (self))
    return TRUE;

  ar = g_object_get_data (G_OBJECT (self), "PNL_DOCK_ITEM_DESCENDANTS");

  if (ar != NULL)
    {
      guint i;

      for (i = 0; i < ar->len; i++)
        {
          PnlDockItem *child = g_ptr_array_index (ar, i);

          if (pnl_dock_item_has_widgets (child))
            return TRUE;
        }
    }

  return FALSE;
}

static void
pnl_dock_item_printf_internal (PnlDockItem *self,
                               GString     *str,
                               guint        depth)
{
  GPtrArray *ar;
  guint i;

  g_assert (PNL_IS_DOCK_ITEM (self));
  g_assert (str != NULL);


  for (i = 0; i < depth; i++)
    g_string_append_c (str, ' ');

  g_string_append_printf (str, "%s\n", G_OBJECT_TYPE_NAME (self));

  ++depth;

  ar = g_object_get_data (G_OBJECT (self), "PNL_DOCK_ITEM_DESCENDANTS");

  if (ar != NULL)
    {
      for (i = 0; i < ar->len; i++)
        pnl_dock_item_printf_internal (g_ptr_array_index (ar, i), str, depth);
    }
}

void
_pnl_dock_item_printf (PnlDockItem *self)
{
  GString *str;

  g_return_if_fail (PNL_IS_DOCK_ITEM (self));

  str = g_string_new (NULL);
  pnl_dock_item_printf_internal (self, str, 0);
  g_printerr ("%s", str->str);
  g_string_free (str, TRUE);
}

/**
 * pnl_dock_item_get_parent:
 *
 * Gets the parent #PnlDockItem, or %NULL.
 *
 * Returns: (transfer none) (nullable): A #PnlDockItem or %NULL.
 */
PnlDockItem *
pnl_dock_item_get_parent (PnlDockItem *self)
{
  GtkWidget *parent;

  g_return_val_if_fail (PNL_IS_DOCK_ITEM (self), NULL);

  for (parent = gtk_widget_get_parent (GTK_WIDGET (self));
       parent != NULL;
       parent = gtk_widget_get_parent (parent))
    {
      if (PNL_IS_DOCK_ITEM (parent))
        return PNL_DOCK_ITEM (parent);
    }

  return NULL;
}

gboolean
pnl_dock_item_get_child_visible (PnlDockItem *self,
                                 PnlDockItem *child)
{
  g_return_val_if_fail (PNL_IS_DOCK_ITEM (self), FALSE);
  g_return_val_if_fail (PNL_IS_DOCK_ITEM (child), FALSE);

  if (PNL_DOCK_ITEM_GET_IFACE (self)->get_child_visible)
    return PNL_DOCK_ITEM_GET_IFACE (self)->get_child_visible (self, child);

  return TRUE;
}

void
pnl_dock_item_set_child_visible (PnlDockItem *self,
                                 PnlDockItem *child,
                                 gboolean     child_visible)
{
  g_return_if_fail (PNL_IS_DOCK_ITEM (self));
  g_return_if_fail (PNL_IS_DOCK_ITEM (child));

  if (PNL_DOCK_ITEM_GET_IFACE (self)->set_child_visible)
    PNL_DOCK_ITEM_GET_IFACE (self)->set_child_visible (self, child, child_visible);
}
