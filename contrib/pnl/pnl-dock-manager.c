/* pnl-dock-manager.c
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

#include "pnl-dock-manager.h"
#include "pnl-dock-transient-grab.h"

typedef struct
{
  GPtrArray *docks;
  PnlDockTransientGrab *grab;
} PnlDockManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (PnlDockManager, pnl_dock_manager, G_TYPE_OBJECT)

enum {
  REGISTER_DOCK,
  UNREGISTER_DOCK,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
pnl_dock_manager_set_focus (PnlDockManager *self,
                            GtkWidget      *focus,
                            GtkWidget      *toplevel)
{
  PnlDockManagerPrivate *priv = pnl_dock_manager_get_instance_private (self);
  PnlDockTransientGrab *grab = NULL;
  GtkWidget *parent;

  g_assert (PNL_IS_DOCK_MANAGER (self));
  g_assert (GTK_IS_WINDOW (toplevel));

  /*
   * Don't do anything if we get a NULL focus. Instead, wait for the focus
   * to be updated with a widget.
   */
  if (focus == NULL)
    return;

#if 0
  g_print ("Attempting to set focus on %s\n", G_OBJECT_TYPE_NAME (focus));
#endif

  if (priv->grab != NULL)
    {
      /*
       * If the current transient grab contains the new focus widget,
       * then there is nothing for us to do now.
       */
      if (pnl_dock_transient_grab_is_descendant (priv->grab, focus))
        return;
    }

  /*
   * If their is a PnlDockItem in the hierarchy, create a new transient grab.
   */
  for (parent = focus;
       parent != NULL;
       parent = gtk_widget_get_parent (parent))
    {
      if (PNL_IS_DOCK_ITEM (parent))
        {
          if (grab == NULL)
            grab = pnl_dock_transient_grab_new ();
          pnl_dock_transient_grab_add_item (grab, PNL_DOCK_ITEM (parent));
        }
    }

  /*
   * Steal common hierarchy so that we don't hide it when breaking grabs.
   */
  if (priv->grab != NULL && grab != NULL)
    pnl_dock_transient_grab_steal_common_ancestors (grab, priv->grab);

  /*
   * Release the previous grab.
   */
  if (priv->grab != NULL)
    {
      pnl_dock_transient_grab_release (priv->grab);
      g_clear_object (&priv->grab);
    }

  /* Start the grab process */
  if (grab != NULL)
    {
      priv->grab = grab;
      pnl_dock_transient_grab_acquire (priv->grab);
    }
}

static void
pnl_dock_manager_hierarchy_changed (PnlDockManager *self,
                                    GtkWidget      *old_toplevel,
                                    GtkWidget      *widget)
{
  GtkWidget *toplevel;

  g_assert (PNL_IS_DOCK_MANAGER (self));
  g_assert (!old_toplevel || GTK_IS_WIDGET (old_toplevel));
  g_assert (GTK_IS_WIDGET (widget));

  if (GTK_IS_WINDOW (old_toplevel))
    g_signal_handlers_disconnect_by_func (old_toplevel,
                                          G_CALLBACK (pnl_dock_manager_set_focus),
                                          self);

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    g_signal_connect_object (toplevel,
                             "set-focus",
                             G_CALLBACK (pnl_dock_manager_set_focus),
                             self,
                             G_CONNECT_SWAPPED);
}

static void
pnl_dock_manager_watch_toplevel (PnlDockManager *self,
                                 GtkWidget      *widget)
{
  g_assert (PNL_IS_DOCK_MANAGER (self));
  g_assert (GTK_IS_WIDGET (widget));

  g_signal_connect_object (widget,
                           "hierarchy-changed",
                           G_CALLBACK (pnl_dock_manager_hierarchy_changed),
                           self,
                           G_CONNECT_SWAPPED);

  pnl_dock_manager_hierarchy_changed (self, NULL, widget);
}

static void
pnl_dock_manager_weak_notify (gpointer  data,
                              GObject  *where_the_object_was)
{
  PnlDockManager *self = data;
  PnlDockManagerPrivate *priv = pnl_dock_manager_get_instance_private (self);

  g_assert (PNL_IS_DOCK_MANAGER (self));

  g_ptr_array_remove (priv->docks, where_the_object_was);
}

static void
pnl_dock_manager_real_register_dock (PnlDockManager *self,
                                     PnlDock        *dock)
{
  PnlDockManagerPrivate *priv = pnl_dock_manager_get_instance_private (self);

  g_return_if_fail (PNL_IS_DOCK_MANAGER (self));
  g_return_if_fail (PNL_IS_DOCK (dock));

  g_object_weak_ref (G_OBJECT (dock), pnl_dock_manager_weak_notify, self);
  g_ptr_array_add (priv->docks, dock);
  pnl_dock_manager_watch_toplevel (self, GTK_WIDGET (dock));
}

static void
pnl_dock_manager_real_unregister_dock (PnlDockManager *self,
                                       PnlDock        *dock)
{
  PnlDockManagerPrivate *priv = pnl_dock_manager_get_instance_private (self);
  guint i;

  g_return_if_fail (PNL_IS_DOCK_MANAGER (self));
  g_return_if_fail (PNL_IS_DOCK (dock));

  for (i = 0; i < priv->docks->len; i++)
    {
      PnlDock *iter = g_ptr_array_index (priv->docks, i);

      if (iter == dock)
        {
          g_object_weak_unref (G_OBJECT (dock), pnl_dock_manager_weak_notify, self);
          g_ptr_array_remove_index (priv->docks, i);
          break;
        }
    }
}

static void
pnl_dock_manager_finalize (GObject *object)
{
  PnlDockManager *self = (PnlDockManager *)object;
  PnlDockManagerPrivate *priv = pnl_dock_manager_get_instance_private (self);

  while (priv->docks->len > 0)
    {
      PnlDock *dock = g_ptr_array_index (priv->docks, priv->docks->len - 1);

      g_object_weak_unref (G_OBJECT (dock), pnl_dock_manager_weak_notify, self);
      g_ptr_array_remove_index (priv->docks, priv->docks->len - 1);
    }

  g_clear_pointer (&priv->docks, g_ptr_array_unref);

  G_OBJECT_CLASS (pnl_dock_manager_parent_class)->finalize (object);
}

static void
pnl_dock_manager_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pnl_dock_manager_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pnl_dock_manager_class_init (PnlDockManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = pnl_dock_manager_finalize;
  object_class->get_property = pnl_dock_manager_get_property;
  object_class->set_property = pnl_dock_manager_set_property;

  klass->register_dock = pnl_dock_manager_real_register_dock;
  klass->unregister_dock = pnl_dock_manager_real_unregister_dock;

  signals [REGISTER_DOCK] =
    g_signal_new ("register-dock",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (PnlDockManagerClass, register_dock),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, PNL_TYPE_DOCK);

  signals [UNREGISTER_DOCK] =
    g_signal_new ("unregister-dock",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (PnlDockManagerClass, unregister_dock),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, PNL_TYPE_DOCK);
}

static void
pnl_dock_manager_init (PnlDockManager *self)
{
  PnlDockManagerPrivate *priv = pnl_dock_manager_get_instance_private (self);

  priv->docks = g_ptr_array_new ();
}

PnlDockManager *
pnl_dock_manager_new (void)
{
  return g_object_new (PNL_TYPE_DOCK_MANAGER, NULL);
}

void
pnl_dock_manager_register_dock (PnlDockManager *self,
                                PnlDock        *dock)
{
  g_return_if_fail (PNL_IS_DOCK_MANAGER (self));
  g_return_if_fail (PNL_IS_DOCK (dock));

  g_signal_emit (self, signals [REGISTER_DOCK], 0, dock);
}

void
pnl_dock_manager_unregister_dock (PnlDockManager *self,
                                  PnlDock        *dock)
{
  g_return_if_fail (PNL_IS_DOCK_MANAGER (self));
  g_return_if_fail (PNL_IS_DOCK (dock));

  g_signal_emit (self, signals [UNREGISTER_DOCK], 0, dock);
}
