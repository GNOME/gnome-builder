/* ide-layout-transient-sidebar.c
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

#define G_LOG_DOMAIN "ide-layout-transient-sidebar"

#include "layout/ide-layout-stack.h"
#include "layout/ide-layout-transient-sidebar.h"

typedef struct
{
  DzlSignalGroup *toplevel_signals;
  GWeakRef        view_ref;
} IdeLayoutTransientSidebarPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeLayoutTransientSidebar,
                            ide_layout_transient_sidebar,
                            IDE_TYPE_LAYOUT_PANE)

static gboolean
has_view_related_focus (IdeLayoutTransientSidebar *self)
{
  IdeLayoutTransientSidebarPrivate *priv = ide_layout_transient_sidebar_get_instance_private (self);
  g_autoptr(IdeLayoutView) view = NULL;
  GtkWidget *focus_view;
  GtkWidget *toplevel;
  GtkWidget *focus;
  GtkWidget *stack;

  g_assert (IDE_IS_LAYOUT_TRANSIENT_SIDEBAR (self));

  /* If there is no view, then nothing more to do */
  view = g_weak_ref_get (&priv->view_ref);
  if (view == NULL)
    return FALSE;

  /* We need the toplevel to get the current focus */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  if (!GTK_IS_WINDOW (toplevel))
    return FALSE;

  /* Synthesize succes when there is no focus, this can happen inbetween
   * various state transitions.
   */
  focus = gtk_window_get_focus (GTK_WINDOW (toplevel));
  if (focus == NULL)
    return TRUE;

  /* If focus is inside this widget, then we don't want to hide */
  if (gtk_widget_is_ancestor (focus, GTK_WIDGET (self)))
    return TRUE;

  /* If focus is in the view, then we definitely don't want to hide */
  if (gtk_widget_is_ancestor (focus, GTK_WIDGET (view)))
    return TRUE;

  /* If the focus has entered another view, then we can release. */
  focus_view = gtk_widget_get_ancestor (focus, IDE_TYPE_LAYOUT_VIEW);
  if (focus_view && focus_view != GTK_WIDGET (view))
    return FALSE;

  /* If the focus has ended up in layout stack, we're okay too */
  stack = dzl_gtk_widget_get_relative (focus, IDE_TYPE_LAYOUT_STACK);
  if (dzl_gtk_widget_is_ancestor_or_relative (focus, stack))
    return TRUE;

  return FALSE;
}

static void
ide_layout_transient_sidebar_after_set_focus (IdeLayoutTransientSidebar *self,
                                              GtkWidget                 *focus,
                                              GtkWindow                 *toplevel)
{
  IdeLayoutTransientSidebarPrivate *priv = ide_layout_transient_sidebar_get_instance_private (self);

  g_assert (IDE_IS_LAYOUT_TRANSIENT_SIDEBAR (self));
  g_assert (GTK_IS_WINDOW (toplevel));

  /*
   * If we are currently visible, then check to see if the focus has gone
   * somewhere outside the panel or the view. If so, we need to dismiss
   * the panel.
   *
   * We try to be tolerant of sibling focus on such things like the stack
   * header.
   */
  if (gtk_widget_get_visible (GTK_WIDGET (self)))
    {
      if (!has_view_related_focus (self))
        {
          dzl_dock_revealer_set_reveal_child (DZL_DOCK_REVEALER (self), FALSE);
          g_weak_ref_set (&priv->view_ref, NULL);
        }
    }
}

static void
ide_layout_transient_sidebar_hierarchy_changed (GtkWidget *widget,
                                                GtkWidget *old_toplevel)
{
  IdeLayoutTransientSidebar *self = (IdeLayoutTransientSidebar *)widget;
  IdeLayoutTransientSidebarPrivate *priv = ide_layout_transient_sidebar_get_instance_private (self);
  GtkWidget *toplevel;

  g_assert (IDE_IS_LAYOUT_TRANSIENT_SIDEBAR (self));
  g_assert (!old_toplevel || GTK_IS_WIDGET (old_toplevel));

  toplevel = gtk_widget_get_toplevel (widget);
  if (!GTK_IS_WINDOW (toplevel))
    toplevel = NULL;

  dzl_signal_group_set_target (priv->toplevel_signals, toplevel);
}

static void
ide_layout_transient_sidebar_finalize (GObject *object)
{
  IdeLayoutTransientSidebar *self = (IdeLayoutTransientSidebar *)object;
  IdeLayoutTransientSidebarPrivate *priv = ide_layout_transient_sidebar_get_instance_private (self);

  g_clear_object (&priv->toplevel_signals);
  g_weak_ref_clear (&priv->view_ref);

  G_OBJECT_CLASS (ide_layout_transient_sidebar_parent_class)->finalize (object);
}

static void
ide_layout_transient_sidebar_class_init (IdeLayoutTransientSidebarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_layout_transient_sidebar_finalize;

  widget_class->hierarchy_changed = ide_layout_transient_sidebar_hierarchy_changed;
}

static void
ide_layout_transient_sidebar_init (IdeLayoutTransientSidebar *self)
{
  IdeLayoutTransientSidebarPrivate *priv = ide_layout_transient_sidebar_get_instance_private (self);

  g_weak_ref_init (&priv->view_ref, NULL);

  priv->toplevel_signals = dzl_signal_group_new (GTK_TYPE_WINDOW);

  dzl_signal_group_connect_data (priv->toplevel_signals,
                                 "set-focus",
                                 G_CALLBACK (ide_layout_transient_sidebar_after_set_focus),
                                 self, NULL,
                                 G_CONNECT_AFTER | G_CONNECT_SWAPPED);

}

/**
 * ide_layout_transient_sidebar_set_view:
 * @self: a #IdeLayoutTransientSidebar
 * @view: (nullable): An #IdeLayoutView or %NULL
 *
 * Sets the view for which the panel is transient for. When focus leaves the
 * sidebar or the view, the panel will be dismissed.
 *
 * Since: 3.26
 */
void
ide_layout_transient_sidebar_set_view (IdeLayoutTransientSidebar *self,
                                       IdeLayoutView             *view)
{
  IdeLayoutTransientSidebarPrivate *priv = ide_layout_transient_sidebar_get_instance_private (self);

  g_return_if_fail (IDE_IS_LAYOUT_TRANSIENT_SIDEBAR (self));
  g_return_if_fail (!view || IDE_IS_LAYOUT_VIEW (view));

  g_weak_ref_set (&priv->view_ref, view);
}
