/* ide-debugger-gutter-renderer.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#include "ide-debug.h"

#include "debugger/ide-debug-manager.h"
#include "debugger/ide-debugger-breakpoints.h"
#include "debugger/ide-debugger-gutter-renderer.h"
#include "debugger/ide-debugger-private.h"

/**
 * SECTION:ide-debugger-gutter-renderer
 * @title: IdeDebuggerGutterRenderer
 * @short_description: A gutter for debugger breakpoints
 *
 * The #IdeDebuggerGutterRenderer is used to show the breakpoints
 * within the gutter of a source editor. When clicking on a row, you
 * can set a breakpoint on the line as well.
 *
 * Since: 3.26
 */

struct _IdeDebuggerGutterRenderer
{
  GtkSourceGutterRendererPixbuf  parent_instance;
  IdeDebuggerBreakpoints        *breakpoints;
  IdeDebugManager               *debug_manager;
  gulong                         breakpoints_changed_handler;
};

G_DEFINE_TYPE (IdeDebuggerGutterRenderer, ide_debugger_gutter_renderer, GTK_SOURCE_TYPE_GUTTER_RENDERER_PIXBUF)

enum {
  PROP_0,
  PROP_BREAKPOINTS,
  PROP_DEBUG_MANAGER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_debugger_gutter_renderer_activate (IdeDebuggerGutterRenderer *self,
                                       const GtkTextIter         *iter,
                                       GdkRectangle              *area,
                                       GdkEvent                  *event)
{
  IdeDebuggerBreakpoint *breakpoint;
  IdeDebuggerBreakMode break_type = IDE_DEBUGGER_BREAK_NONE;
  g_autofree gchar *path = NULL;
  GFile *file;
  guint line;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER_GUTTER_RENDERER (self));
  g_assert (self->breakpoints != NULL);
  g_assert (self->debug_manager != NULL);
  g_assert (iter != NULL);
  g_assert (area != NULL);
  g_assert (event != NULL);

  line = gtk_text_iter_get_line (iter) + 1;
  file = ide_debugger_breakpoints_get_file (self->breakpoints);
  path = g_file_get_path (file);

  /* TODO: Should we show a Popover here to select the type? */
  IDE_TRACE_MSG ("Toggle breakpoint on line %u [breakpoints=%p]",
                 line, self->breakpoints);

  breakpoint = ide_debugger_breakpoints_get_line (self->breakpoints, line);
  if (breakpoint != NULL)
    break_type = ide_debugger_breakpoint_get_mode (breakpoint);

  switch (break_type)
    {
    case IDE_DEBUGGER_BREAK_NONE:
      {
        g_autoptr(IdeDebuggerBreakpoint) to_insert = NULL;

        to_insert = ide_debugger_breakpoint_new (NULL);

        ide_debugger_breakpoint_set_line (to_insert, line);
        ide_debugger_breakpoint_set_file (to_insert, path);
        ide_debugger_breakpoint_set_mode (to_insert, IDE_DEBUGGER_BREAK_BREAKPOINT);
        ide_debugger_breakpoint_set_enabled (to_insert, TRUE);

        _ide_debug_manager_add_breakpoint (self->debug_manager, to_insert);
      }
      break;

    case IDE_DEBUGGER_BREAK_BREAKPOINT:
    case IDE_DEBUGGER_BREAK_COUNTPOINT:
    case IDE_DEBUGGER_BREAK_WATCHPOINT:
      if (breakpoint != NULL)
        _ide_debug_manager_remove_breakpoint (self->debug_manager, breakpoint);
      break;

    default:
      g_return_if_reached ();
    }

  /*
   * We will wait for changes to be applied to the #IdeDebuggerBreakpoints
   * by the #IdeDebugManager. That will cause the gutter to be invalidated
   * and redrawn.
   */

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

  return self->breakpoints != NULL && self->debug_manager != NULL;
}

static void
ide_debugger_gutter_renderer_query_data (IdeDebuggerGutterRenderer    *self,
                                         const GtkTextIter            *begin,
                                         const GtkTextIter            *end,
                                         GtkSourceGutterRendererState  state)
{
  IdeDebuggerBreakMode break_type;
  guint line;

  g_assert (IDE_IS_DEBUGGER_GUTTER_RENDERER (self));
  g_assert (begin != NULL);
  g_assert (end != NULL);

  if (self->breakpoints == NULL)
    return;

  line = gtk_text_iter_get_line (begin) + 1;
  break_type = ide_debugger_breakpoints_get_line_mode (self->breakpoints, line);

  /*
   * These are very much a miss-appropriation of the icon, but it works
   * well enough for now until we get real symbolic icons for these.
   */
#define BREAKPOINT_ICON_NAME "debug-breakpoint-symbolic"
#define COUNTPOINT_ICON_NAME "debug-breakpoint-symbolic"
#define WATCHPOINT_ICON_NAME "debug-breakpoint-symbolic"

  switch (break_type)
    {
    case IDE_DEBUGGER_BREAK_BREAKPOINT:
      gtk_source_gutter_renderer_pixbuf_set_icon_name (GTK_SOURCE_GUTTER_RENDERER_PIXBUF (self),
                                                       BREAKPOINT_ICON_NAME);
      break;

    case IDE_DEBUGGER_BREAK_COUNTPOINT:
      gtk_source_gutter_renderer_pixbuf_set_icon_name (GTK_SOURCE_GUTTER_RENDERER_PIXBUF (self),
                                                       COUNTPOINT_ICON_NAME);
      break;

    case IDE_DEBUGGER_BREAK_WATCHPOINT:
      gtk_source_gutter_renderer_pixbuf_set_icon_name (GTK_SOURCE_GUTTER_RENDERER_PIXBUF (self),
                                                       WATCHPOINT_ICON_NAME);
      break;

    case IDE_DEBUGGER_BREAK_NONE:
      /* FIXME: It would be nice if we could apply an alpha here, but seems to
       *        require more rendering code than I want to deal with right now.
       */
      if ((state & GTK_SOURCE_GUTTER_RENDERER_STATE_PRELIT) != 0)
        gtk_source_gutter_renderer_pixbuf_set_icon_name (GTK_SOURCE_GUTTER_RENDERER_PIXBUF (self),
                                                         BREAKPOINT_ICON_NAME);
      else
        /* Setting pixbuf to NULL via g_object_set() seems to be
         * the only way to clear this without g_warning()s.
         */
        g_object_set (self, "pixbuf", NULL, NULL);
      break;

    default:
      g_return_if_reached ();
    }

#undef BREAKPOINT_ICON_NAME
#undef COUNTPOINT_ICON_NAME
#undef WATCHPOINT_ICON_NAME
}

static void
ide_debugger_gutter_renderer_breakpoints_changed (IdeDebuggerGutterRenderer *self,
                                                  IdeDebuggerBreakpoints    *breakpoints)
{
  g_assert (IDE_IS_DEBUGGER_GUTTER_RENDERER (self));
  g_assert (IDE_IS_DEBUGGER_BREAKPOINTS (breakpoints));

  gtk_source_gutter_renderer_queue_draw (GTK_SOURCE_GUTTER_RENDERER (self));
}

void
ide_debugger_gutter_renderer_set_breakpoints (IdeDebuggerGutterRenderer *self,
                                              IdeDebuggerBreakpoints    *breakpoints)
{
  g_assert (IDE_IS_DEBUGGER_GUTTER_RENDERER (self));
  g_assert (!breakpoints || IDE_IS_DEBUGGER_BREAKPOINTS (breakpoints));

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
ide_debugger_gutter_renderer_dispose (GObject *object)
{
  IdeDebuggerGutterRenderer *self = (IdeDebuggerGutterRenderer *)object;

  ide_debugger_gutter_renderer_set_breakpoints (self, NULL);
  g_clear_object (&self->debug_manager);

  G_OBJECT_CLASS (ide_debugger_gutter_renderer_parent_class)->dispose (object);
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

    case PROP_DEBUG_MANAGER:
      g_value_set_object (value, self->debug_manager);
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

    case PROP_DEBUG_MANAGER:
      self->debug_manager = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_gutter_renderer_class_init (IdeDebuggerGutterRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_debugger_gutter_renderer_dispose;
  object_class->get_property = ide_debugger_gutter_renderer_get_property;
  object_class->set_property = ide_debugger_gutter_renderer_set_property;

  /**
   * IdeDebuggerGutterRenderer:breakpoints:
   *
   * The "breakpoints" property is an #IdeDebuggerBreakpoints that can be
   * used to quickly determine if a row has a breakpoint set, and what
   * type of breakpoint that is.
   *
   * Since: 3.26
   */
  properties [PROP_BREAKPOINTS] =
    g_param_spec_object ("breakpoints",
                         "Breakpoints",
                         "Breakpoints",
                         IDE_TYPE_DEBUGGER_BREAKPOINTS,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeDebuggerGutterRenderer:debug-manager:
   *
   * The "debug-manager" property is the #IdeDebugManager that can be
   * used to alter breakpoints in the debugger.
   *
   * Since: 3.26
   */
  properties [PROP_DEBUG_MANAGER] =
    g_param_spec_object ("debug-manager",
                         "Debug Manager",
                         "Debug Manager",
                         IDE_TYPE_DEBUG_MANAGER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

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

/**
 * ide_debugger_gutter_renderer_new:
 * @debug_manager: An #IdeDebugManager
 *
 * Creates a new #IdeDebuggerGutterRenderer.
 *
 * @debug_manager should be the #IdeDebugManager for the #IdeContext of the
 * current project. This is used to manipulate breakpoints whether or not
 * the debugger is currently active. This allows for breakpoints to be synced
 * after the debugger instance is spawned.
 *
 * Returns: (transfer full): A new #IdeDebuggerGutterRenderer.
 *
 * Since: 3.26
 */
GtkSourceGutterRenderer *
ide_debugger_gutter_renderer_new (IdeDebugManager *debug_manager)
{
  g_return_val_if_fail (IDE_IS_DEBUG_MANAGER (debug_manager), NULL);

  return g_object_new (IDE_TYPE_DEBUGGER_GUTTER_RENDERER,
                       "debug-manager", debug_manager,
                       NULL);
}
