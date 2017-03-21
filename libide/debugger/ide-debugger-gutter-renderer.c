/* ide-debugger-gutter-renderer.c
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

#define G_LOG_DOMAIN "ide-debugger-gutter-renderer"

#include <glib/gi18n.h>

#include "ide-debugger-gutter-renderer.h"
#include "ide-debugger-breakpoints.h"

struct _IdeDebuggerGutterRenderer
{
  GtkSourceGutterRendererPixbuf  parent_instance;
  IdeDebuggerBreakpoints        *breakpoints;
  gulong                         breakpoints_changed_handler;
};

G_DEFINE_TYPE (IdeDebuggerGutterRenderer, ide_debugger_gutter_renderer, GTK_SOURCE_TYPE_GUTTER_RENDERER_PIXBUF)

enum {
  PROP_0,
  PROP_BREAKPOINTS,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_debugger_gutter_renderer_activate (IdeDebuggerGutterRenderer *self,
                                       const GtkTextIter         *iter,
                                       GdkRectangle              *area,
                                       GdkEvent                  *event)
{
  IdeDebuggerBreakType break_type;
  guint line;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER_GUTTER_RENDERER (self));
  g_assert (iter != NULL);
  g_assert (area != NULL);
  g_assert (event != NULL);

  line = gtk_text_iter_get_line (iter) + 1;

  IDE_TRACE_MSG ("Toggle breakpoint on line %u", line);

  break_type = ide_debugger_breakpoints_lookup (self->breakpoints, line);

  switch (break_type)
    {
    case IDE_DEBUGGER_BREAK_NONE:
      ide_debugger_breakpoints_add (self->breakpoints, line, IDE_DEBUGGER_BREAK_BREAKPOINT);
      break;

    case IDE_DEBUGGER_BREAK_BREAKPOINT:
    case IDE_DEBUGGER_BREAK_COUNTPOINT:
    case IDE_DEBUGGER_BREAK_WATCHPOINT:
      ide_debugger_breakpoints_remove (self->breakpoints, line);
      break;

    default:
      break;
    }

  IDE_EXIT;
}

static gboolean
ide_debugger_gutter_renderer_query_activatable (IdeDebuggerGutterRenderer *self,
                                                const GtkTextIter         *begin,
                                                const GdkRectangle        *area,
                                                GdkEvent                  *event)
{
  g_assert (IDE_IS_DEBUGGER_GUTTER_RENDERER (self));
  g_assert (begin != NULL);
  g_assert (area != NULL);
  g_assert (event != NULL);

  return TRUE;
}

static void
ide_debugger_gutter_renderer_query_data (IdeDebuggerGutterRenderer    *self,
                                         const GtkTextIter            *begin,
                                         const GtkTextIter            *end,
                                         GtkSourceGutterRendererState  state)
{
  IdeDebuggerBreakType break_type;
  guint line;

  g_assert (IDE_IS_DEBUGGER_GUTTER_RENDERER (self));
  g_assert (begin != NULL);
  g_assert (end != NULL);

  if (self->breakpoints == NULL)
    return;

  line = gtk_text_iter_get_line (begin) + 1;
  break_type = ide_debugger_breakpoints_lookup (self->breakpoints, line);

  /*
   * These are very much a miss-appropriation of the icon, but it works
   * well enough for now until we get real symbolic icons for these.
   */
#define BREAKPOINT_ICON_NAME "edit-clear-symbolic-rtl"
#define COUNTPOINT_ICON_NAME "edit-clear-symbolic-rtl"
#define WATCHPOINT_ICON_NAME "edit-clear-symbolic-rtl"

  switch (break_type)
    {
    case IDE_DEBUGGER_BREAK_BREAKPOINT:
      gtk_source_gutter_renderer_pixbuf_set_icon_name (GTK_SOURCE_GUTTER_RENDERER_PIXBUF (self), BREAKPOINT_ICON_NAME);
      break;

    case IDE_DEBUGGER_BREAK_COUNTPOINT:
      gtk_source_gutter_renderer_pixbuf_set_icon_name (GTK_SOURCE_GUTTER_RENDERER_PIXBUF (self), COUNTPOINT_ICON_NAME);
      break;

    case IDE_DEBUGGER_BREAK_WATCHPOINT:
      gtk_source_gutter_renderer_pixbuf_set_icon_name (GTK_SOURCE_GUTTER_RENDERER_PIXBUF (self), WATCHPOINT_ICON_NAME);
      break;

    case IDE_DEBUGGER_BREAK_NONE:
    default:
      /* Setting pixbuf to NULL via g_object_set() seems to be
       * the only way to clear this without g_warning()s.
       */
      g_object_set (self, "pixbuf", NULL, NULL);
      break;
    }
}

static void
ide_debugger_gutter_renderer_breakpoints_changed (IdeDebuggerGutterRenderer *self,
                                                  IdeDebuggerBreakpoints    *breakpoints)
{
  g_assert (IDE_IS_DEBUGGER_GUTTER_RENDERER (self));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINTS (breakpoints));

  gtk_source_gutter_renderer_queue_draw (GTK_SOURCE_GUTTER_RENDERER (self));
}

static void
ide_debugger_gutter_renderer_set_breakpoints (IdeDebuggerGutterRenderer *self,
                                              IdeDebuggerBreakpoints    *breakpoints)
{
  g_return_if_fail (IDE_IS_DEBUGGER_GUTTER_RENDERER (self));
  g_return_if_fail (!breakpoints || IDE_IS_DEBUGGER_BREAKPOINTS (breakpoints));

  if (self->breakpoints != breakpoints)
    {
      if (self->breakpoints != NULL)
        {
          g_signal_handler_disconnect (self->breakpoints, self->breakpoints_changed_handler);
          self->breakpoints_changed_handler = 0;
          g_clear_object (&self->breakpoints);
        }

      if (breakpoints != NULL)
        {
          self->breakpoints = g_object_ref (breakpoints);
          self->breakpoints_changed_handler =
            g_signal_connect_object (breakpoints,
                                     "changed",
                                     G_CALLBACK (ide_debugger_gutter_renderer_breakpoints_changed),
                                     self,
                                     G_CONNECT_SWAPPED);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BREAKPOINTS]);
    }
}

static void
ide_debugger_gutter_renderer_finalize (GObject *object)
{
  IdeDebuggerGutterRenderer *self = (IdeDebuggerGutterRenderer *)object;

  g_clear_object (&self->breakpoints);

  G_OBJECT_CLASS (ide_debugger_gutter_renderer_parent_class)->finalize (object);
}

static void
ide_debugger_gutter_renderer_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  IdeDebuggerGutterRenderer *self = IDE_DEBUGGER_GUTTER_RENDERER (object);

  switch (prop_id)
    {
    case PROP_BREAKPOINTS:
      g_value_set_object (value, self->breakpoints);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_gutter_renderer_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  IdeDebuggerGutterRenderer *self = IDE_DEBUGGER_GUTTER_RENDERER (object);

  switch (prop_id)
    {
    case PROP_BREAKPOINTS:
      ide_debugger_gutter_renderer_set_breakpoints (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_gutter_renderer_class_init (IdeDebuggerGutterRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_debugger_gutter_renderer_finalize;
  object_class->get_property = ide_debugger_gutter_renderer_get_property;
  object_class->set_property = ide_debugger_gutter_renderer_set_property;

  properties [PROP_BREAKPOINTS] =
    g_param_spec_object ("breakpoints",
                         "Breakpoints",
                         "Breakpoints",
                         IDE_TYPE_DEBUGGER_BREAKPOINTS,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_debugger_gutter_renderer_init (IdeDebuggerGutterRenderer *self)
{
  g_signal_connect (self,
                    "activate",
                    G_CALLBACK (ide_debugger_gutter_renderer_activate),
                    NULL);

  g_signal_connect (self,
                    "query-activatable",
                    G_CALLBACK (ide_debugger_gutter_renderer_query_activatable),
                    NULL);

  g_signal_connect (self,
                    "query-data",
                    G_CALLBACK (ide_debugger_gutter_renderer_query_data),
                    NULL);
}

GtkSourceGutterRenderer *
ide_debugger_gutter_renderer_new (void)
{
  return g_object_new (IDE_TYPE_DEBUGGER_GUTTER_RENDERER, NULL);
}
