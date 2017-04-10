/* ide-debugger-view.c
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

#define G_LOG_DOMAIN "ide-debugger-view"

#include <gtksourceview/gtksource.h>

#include "ide-debug.h"

#include "debugger/ide-debugger-view.h"

struct _IdeDebuggerView
{
  IdeLayoutView    parent_instance;
  GtkSourceView   *source_view;
};

enum {
  PROP_0,
  PROP_BUFFER,
  N_PROPS
};

G_DEFINE_TYPE (IdeDebuggerView, ide_debugger_view, IDE_TYPE_LAYOUT_VIEW)

static GParamSpec *properties [N_PROPS];

static void
ide_debugger_view_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeDebuggerView *self = IDE_DEBUGGER_VIEW (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      g_value_set_object (value, ide_debugger_view_get_buffer (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_view_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeDebuggerView *self = IDE_DEBUGGER_VIEW (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      ide_debugger_view_set_buffer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_view_class_init (IdeDebuggerViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ide_debugger_view_get_property;
  object_class->set_property = ide_debugger_view_set_property;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The buffer for the view",
                         GTK_SOURCE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-debugger-view.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeDebuggerView, source_view);
}

static void
ide_debugger_view_init (IdeDebuggerView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ide_debugger_view_new (void)
{
  return g_object_new (IDE_TYPE_DEBUGGER_VIEW, NULL);
}

/**
 * ide_debugger_view_get_buffer:
 * @self: a #IdeDebuggerView
 *
 * Gets the buffer for the view.
 *
 * Returns: (transfer none): A #GtkSourceBuffer
 */
GtkSourceBuffer *
ide_debugger_view_get_buffer (IdeDebuggerView *self)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER_VIEW (self), NULL);

  return GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view)));
}

void
ide_debugger_view_set_buffer (IdeDebuggerView *self,
                              GtkSourceBuffer *buffer)
{
  g_return_if_fail (IDE_IS_DEBUGGER_VIEW (self));
  g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

  if (buffer != ide_debugger_view_get_buffer (self))
    {
      gtk_text_view_set_buffer (GTK_TEXT_VIEW (self->source_view),
                                GTK_TEXT_BUFFER (buffer));
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUFFER]);
    }
}
