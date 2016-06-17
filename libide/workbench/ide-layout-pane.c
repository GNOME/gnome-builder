/* ide-layout-pane.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "egg-signal-group.h"

#include "ide-layout-pane.h"
#include "ide-workbench.h"
#include "ide-workbench-private.h"
#include "ide-macros.h"

struct _IdeLayoutPane
{
  PnlDockBinEdge    parent_instance;

  EggSignalGroup   *toplevel_signals;

  PnlDockStack     *dock_stack;
};

G_DEFINE_TYPE (IdeLayoutPane, ide_layout_pane, PNL_TYPE_DOCK_BIN_EDGE)

static void
ide_layout_pane_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  IdeLayoutPane *self = (IdeLayoutPane *)container;

  g_assert (IDE_IS_LAYOUT_PANE (self));

  if (PNL_IS_DOCK_WIDGET (widget))
    gtk_container_add (GTK_CONTAINER (self->dock_stack), widget);
  else
    GTK_CONTAINER_CLASS (ide_layout_pane_parent_class)->add (container, widget);
}

static void
workbench_focus_changed (GtkWidget     *toplevel,
                         GtkWidget     *focus,
                         IdeLayoutPane *self)
{
  GtkStyleContext *style_context;
  GtkWidget *parent;

  g_assert (GTK_IS_WIDGET (toplevel));
  g_assert (!focus || GTK_IS_WIDGET (focus));
  g_assert (IDE_IS_LAYOUT_PANE (self));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));

  parent = focus;

  while (parent && (parent != (GtkWidget *)self))
    {
      if (GTK_IS_POPOVER (parent))
        parent = gtk_popover_get_relative_to (GTK_POPOVER (parent));
      else
        parent = gtk_widget_get_parent (parent);
    }

  if (parent == NULL)
    gtk_style_context_remove_class (style_context, "focus");
  else
    gtk_style_context_add_class (style_context, "focus");
}

static void
ide_layout_pane_hierarchy_changed (GtkWidget *widget,
                                   GtkWidget *old_parent)
{
  IdeLayoutPane *self = (IdeLayoutPane *)widget;
  GtkWidget *toplevel;

  g_assert (IDE_IS_LAYOUT_PANE (self));

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  if (!GTK_IS_WINDOW (toplevel))
    toplevel = NULL;

  egg_signal_group_set_target (self->toplevel_signals, toplevel);
}

static void
ide_layout_pane_dispose (GObject *object)
{
  IdeLayoutPane *self = (IdeLayoutPane *)object;

  g_clear_object (&self->toplevel_signals);

  G_OBJECT_CLASS (ide_layout_pane_parent_class)->dispose (object);
}

static void
ide_layout_pane_class_init (IdeLayoutPaneClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->dispose = ide_layout_pane_dispose;

  widget_class->hierarchy_changed = ide_layout_pane_hierarchy_changed;

  container_class->add = ide_layout_pane_add;

  gtk_widget_class_set_css_name (widget_class, "layoutpane");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-layout-pane.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutPane, dock_stack);
}

static void
ide_layout_pane_init (IdeLayoutPane *self)
{
  self->toplevel_signals = egg_signal_group_new (GTK_TYPE_WINDOW);
  egg_signal_group_connect_object (self->toplevel_signals,
                                   "set-focus",
                                   G_CALLBACK (workbench_focus_changed),
                                   self,
                                   G_CONNECT_AFTER);

  gtk_widget_init_template (GTK_WIDGET (self));
}
