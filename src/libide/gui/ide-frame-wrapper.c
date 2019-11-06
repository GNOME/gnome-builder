/* ide-frame-wrapper.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-frame-wrapper"

#include "config.h"

#include "ide-frame-wrapper.h"

/*
 * This is just a GtkStack wrapper that allows us to override
 * GtkContainer::remove() so that we can transition to the previously
 * focused child first.
 */

struct _IdeFrameWrapper
{
  GtkStack parent_instance;
  GQueue   history;
};

G_DEFINE_TYPE (IdeFrameWrapper, ide_frame_wrapper, GTK_TYPE_STACK)

static void
ide_frame_wrapper_add (GtkContainer *container,
                       GtkWidget    *widget)
{
  IdeFrameWrapper *self = (IdeFrameWrapper *)container;

  g_assert (IDE_IS_FRAME_WRAPPER (container));
  g_assert (GTK_IS_WIDGET (widget));

  g_object_freeze_notify (G_OBJECT (self));

  if (gtk_widget_get_visible (widget))
    g_queue_push_head (&self->history, widget);
  else
    g_queue_push_tail (&self->history, widget);

  GTK_CONTAINER_CLASS (ide_frame_wrapper_parent_class)->add (container, widget);

  g_object_thaw_notify (G_OBJECT (self));
}

static void
ide_frame_wrapper_remove (GtkContainer *container,
                          GtkWidget    *widget)
{
  IdeFrameWrapper *self = (IdeFrameWrapper *)container;

  g_assert (IDE_IS_FRAME_WRAPPER (container));
  g_assert (GTK_IS_WIDGET (widget));

  /* Remove the widget from our history chain, and then see if we need to
   * first change the visible child before removing. If we don't we risk,
   * focusing the wrong "next" widget as part of the removal.
   */

  g_object_freeze_notify (G_OBJECT (self));

  g_queue_remove (&self->history, widget);

  if (self->history.length > 0)
    {
      GtkWidget *new_fg = g_queue_peek_head (&self->history);

      if (new_fg != gtk_stack_get_visible_child (GTK_STACK (self)))
        gtk_stack_set_visible_child (GTK_STACK (self), new_fg);
    }

  GTK_CONTAINER_CLASS (ide_frame_wrapper_parent_class)->remove (container, widget);

  g_object_thaw_notify (G_OBJECT (self));
}

static void
ide_frame_wrapper_notify_visible_child (IdeFrameWrapper *self,
                                        GParamSpec      *pspec)
{
  GtkWidget *visible_child;

  g_assert (IDE_IS_FRAME_WRAPPER (self));
  g_assert (pspec != NULL);

  if ((visible_child = gtk_stack_get_visible_child (GTK_STACK (self))))
    {
      if (visible_child != g_queue_peek_head (&self->history))
        {
          GList *link_ = g_queue_find (&self->history, visible_child);

          g_assert (link_ != NULL);

          g_queue_unlink (&self->history, link_);
          g_queue_push_head_link (&self->history, link_);
        }
    }
}

static void
ide_frame_wrapper_class_init (IdeFrameWrapperClass *klass)
{
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  container_class->add = ide_frame_wrapper_add;
  container_class->remove = ide_frame_wrapper_remove;
}

static void
ide_frame_wrapper_init (IdeFrameWrapper *self)
{
  g_signal_connect (self,
                    "notify::visible-child",
                    G_CALLBACK (ide_frame_wrapper_notify_visible_child),
                    NULL);
}
