/* gb-view-stack.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <ide.h>

#include "gb-view.h"
#include "gb-view-grid.h"
#include "gb-view-stack.h"
#include "gb-view-stack-actions.h"
#include "gb-view-stack-private.h"
#include "gb-widget.h"

G_DEFINE_TYPE (GbViewStack, gb_view_stack, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_ACTIVE_VIEW,
  LAST_PROP
};

enum {
  EMPTY,
  SPLIT,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

static void
gb_view_stack_add (GtkContainer *container,
                   GtkWidget    *child)
{
  GbViewStack *self = (GbViewStack *)container;

  g_assert (GB_IS_VIEW_STACK (self));

  if (GB_IS_VIEW (child))
    {
      GtkWidget *controls;

      gtk_widget_set_sensitive (GTK_WIDGET (self->close_button), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->document_button), TRUE);

      self->focus_history = g_list_prepend (self->focus_history, child);
      controls = gb_view_get_controls (GB_VIEW (child));
      if (controls)
        gtk_container_add (GTK_CONTAINER (self->controls_stack), controls);
      gtk_container_add (GTK_CONTAINER (self->stack), child);
      gtk_stack_set_visible_child (self->stack, child);
    }
  else
    {
      GTK_CONTAINER_CLASS (gb_view_stack_parent_class)->add (container, child);
    }
}

void
gb_view_stack_remove (GbViewStack *self,
                      GbView      *view)
{
  GtkWidget *controls;

  g_assert (GB_IS_VIEW_STACK (self));
  g_assert (GB_IS_VIEW (view));

  self->focus_history = g_list_remove (self->focus_history, view);
  controls = gb_view_get_controls (view);
  if (controls)
    gtk_container_remove (GTK_CONTAINER (self->controls_stack), controls);
  gtk_container_remove (GTK_CONTAINER (self->stack), GTK_WIDGET (view));

  if (self->focus_history)
    {
      GtkWidget *child;

      child = self->focus_history->data;
      gtk_stack_set_visible_child (self->stack, child);
      gtk_widget_grab_focus (GTK_WIDGET (child));
    }
  else
    g_signal_emit (self, gSignals [EMPTY], 0);
}

static void
gb_view_stack_real_remove (GtkContainer *container,
                           GtkWidget    *child)
{
  GbViewStack *self = (GbViewStack *)container;

  g_assert (GB_IS_VIEW_STACK (self));

  if (GB_IS_VIEW (child))
    gb_view_stack_remove (self, GB_VIEW (child));
  else
    GTK_CONTAINER_CLASS (gb_view_stack_parent_class)->remove (container, child);
}

static void
gb_view_stack__notify_visible_child (GbViewStack *self,
                                     GParamSpec  *pspec,
                                     GtkStack    *stack)
{
  GtkWidget *visible_child;

  g_assert (GB_IS_VIEW_STACK (self));
  g_assert (GTK_IS_STACK (stack));

  visible_child = gtk_stack_get_visible_child (stack);

  gb_view_stack_set_active_view (self, visible_child);
}

static void
gb_view_stack_grab_focus (GtkWidget *widget)
{
  GbViewStack *self = (GbViewStack *)widget;
  GtkWidget *visible_child;

  g_assert (GB_IS_VIEW_STACK (self));

  visible_child = gtk_stack_get_visible_child (self->stack);
  if (visible_child)
    gtk_widget_grab_focus (visible_child);
}

static gboolean
gb_view_stack_is_empty (GbViewStack *self)
{
  g_return_val_if_fail (GB_IS_VIEW_STACK (self), FALSE);

  return (self->focus_history == NULL);
}

static void
gb_view_stack_real_empty (GbViewStack *self)
{
  g_assert (GB_IS_VIEW_STACK (self));

  /* its possible for a widget to be added during "empty" emission. */
  if (gb_view_stack_is_empty (self))
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->close_button), FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->document_button), FALSE);
    }
}

static void
gb_view_stack_constructed (GObject *object)
{
  GbViewStack *self = (GbViewStack *)object;

  G_OBJECT_CLASS (gb_view_stack_parent_class)->constructed (object);

  gb_view_stack_actions_init (self);
}

static void
gb_view_stack_finalize (GObject *object)
{
  GbViewStack *self = (GbViewStack *)object;

  g_clear_pointer (&self->focus_history, g_list_free);
  ide_clear_weak_pointer (&self->title_binding);
  ide_clear_weak_pointer (&self->active_view);

  G_OBJECT_CLASS (gb_view_stack_parent_class)->finalize (object);
}

static void
gb_view_stack_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GbViewStack *self = GB_VIEW_STACK (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_VIEW:
      g_value_set_object (value, gb_view_stack_get_active_view (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_view_stack_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GbViewStack *self = GB_VIEW_STACK (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_VIEW:
      gb_view_stack_set_active_view (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_view_stack_class_init (GbViewStackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->constructed = gb_view_stack_constructed;
  object_class->finalize = gb_view_stack_finalize;
  object_class->get_property = gb_view_stack_get_property;
  object_class->set_property = gb_view_stack_set_property;

  widget_class->grab_focus = gb_view_stack_grab_focus;

  container_class->add = gb_view_stack_add;
  container_class->remove = gb_view_stack_real_remove;

  gParamSpecs [PROP_ACTIVE_VIEW] =
    g_param_spec_object ("active-view",
                         _("Active View"),
                         _("The active view."),
                         GB_TYPE_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ACTIVE_VIEW, gParamSpecs [PROP_ACTIVE_VIEW]);

  gSignals [EMPTY] =
    g_signal_new_class_handler ("empty",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (gb_view_stack_real_empty),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE,
                                0);

  gSignals [SPLIT] = g_signal_new ("split",
                                   G_TYPE_FROM_CLASS (klass),
                                   G_SIGNAL_RUN_LAST,
                                   0,
                                   NULL, NULL,
                                   g_cclosure_marshal_generic,
                                   G_TYPE_NONE,
                                   2,
                                   GB_TYPE_VIEW,
                                   GB_TYPE_VIEW_GRID_SPLIT);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-view-stack.ui");
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, close_button);
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, controls_stack);
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, document_button);
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, go_backward);
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, go_forward);
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, popover);
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, stack);
  GB_WIDGET_CLASS_BIND (klass, GbViewStack, title_label);
}

static void
gb_view_stack_init (GbViewStack *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->stack,
                           "notify::visible-child",
                           G_CALLBACK (gb_view_stack__notify_visible_child),
                           self,
                           G_CONNECT_SWAPPED);
}

GtkWidget *
gb_view_stack_new (void)
{
  return g_object_new (GB_TYPE_VIEW_STACK, NULL);
}

GtkWidget *
gb_view_stack_get_active_view (GbViewStack *self)
{
  g_return_val_if_fail (GB_IS_VIEW_STACK (self), NULL);

  return self->active_view;
}

void
gb_view_stack_set_active_view (GbViewStack *self,
                               GtkWidget   *active_view)
{
  g_return_if_fail (GB_IS_VIEW_STACK (self));
  g_return_if_fail (!active_view || GB_IS_VIEW (active_view));

  if (self->active_view != active_view)
    {
      if (self->active_view)
        {
          if (self->title_binding)
            g_binding_unbind (self->title_binding);
          ide_clear_weak_pointer (&self->title_binding);
          gtk_label_set_label (self->title_label, NULL);
          ide_clear_weak_pointer (&self->active_view);
          gtk_widget_hide (GTK_WIDGET (self->controls_stack));
        }

      if (active_view)
        {
          GtkWidget *controls;
          GBinding *binding;
          GActionGroup *group;

          self->focus_history = g_list_remove (self->focus_history, active_view);
          self->focus_history = g_list_prepend (self->focus_history, active_view);

          if (active_view != gtk_stack_get_visible_child (self->stack))
            gtk_stack_set_visible_child (self->stack, active_view);
          binding = g_object_bind_property (active_view, "title",
                                            self->title_label, "label",
                                            G_BINDING_SYNC_CREATE);
          ide_set_weak_pointer (&self->title_binding, binding);
          ide_set_weak_pointer (&self->active_view, active_view);
          controls = gb_view_get_controls (GB_VIEW (active_view));
          if (controls)
            {
              gtk_stack_set_visible_child (self->controls_stack, controls);
              gtk_widget_show (GTK_WIDGET (self->controls_stack));
            }
          group = gtk_widget_get_action_group (active_view, "view");
          if (group)
            gtk_widget_insert_action_group (GTK_WIDGET (self), "view", group);
        }

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_ACTIVE_VIEW]);
    }
}

GtkWidget *
gb_view_stack_find_with_document (GbViewStack *self,
                                  GbDocument  *document)
{
  GtkWidget *ret = NULL;
  GList *iter;
  GList *children;

  g_return_val_if_fail (GB_IS_VIEW_STACK (self), NULL);
  g_return_val_if_fail (GB_IS_DOCUMENT (document), NULL);

  children = gtk_container_get_children (GTK_CONTAINER (self->stack));

  for (iter = children; iter; iter = iter->next)
    {
      GbView *view = iter->data;
      GbDocument *item;

      g_assert (GB_IS_VIEW (view));

      item = gb_view_get_document (view);

      if (item == document)
        {
          ret = GTK_WIDGET (view);
          break;
        }
    }

  g_list_free (children);

  return ret;
}

void
gb_view_stack_focus_document (GbViewStack *self,
                              GbDocument  *document)
{
  GtkWidget *view;

  g_return_if_fail (GB_IS_VIEW_STACK (self));
  g_return_if_fail (GB_IS_DOCUMENT (document));

  view = gb_view_stack_find_with_document (self, document);

  if (view != NULL && GB_IS_VIEW (view))
    {
      gb_view_stack_set_active_view (self, view);
      gtk_widget_grab_focus (view);
      return;
    }

  view = gb_document_create_view (document);

  if (view == NULL)
    {
      g_warning ("Document %s failed to create a view",
                 gb_document_get_title (document));
      return;
    }

  if (!GB_IS_VIEW (view))
    {
      g_warning ("Document %s did not create a GbView instance.",
                 gb_document_get_title (document));
      return;
    }

  gb_view_stack_add (GTK_CONTAINER (self), view);
  gb_view_stack_set_active_view (self, view);
  gtk_widget_grab_focus (view);
}
