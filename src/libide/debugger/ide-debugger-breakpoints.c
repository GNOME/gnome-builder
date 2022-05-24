/* ide-debugger-breakpoints.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-debugger-breakpoints"

#include "config.h"

#include <stdlib.h>

#include "ide-debugger-breakpoints.h"
#include "ide-debugger-private.h"

/**
 * SECTION:ide-debugger-breakpoints
 * @title: IdeDebuggerBreakpoints
 * @short_title: A collection of breakpoints for a file
 *
 * The #IdeDebuggerBreakpoints provides a convenient container for breakpoints
 * about a single file. This is useful for situations like the document gutter
 * where we need very fast access to whether or not a line has a breakpoint set
 * during the rendering process.
 *
 * At it's core, this is a sparse array as rarely do we have more than one
 * cacheline of information about breakpoints in a file.
 *
 * This object is controled by the IdeDebuggerManager and will modify the
 * breakpoints as necessary by the current debugger. If no debugger is
 * active, the breakpoints are queued until the debugger has started, and
 * then synchronized to the debugger process.
 */

typedef struct
{
  guint                  line;
  IdeDebuggerBreakMode   mode;
  IdeDebuggerBreakpoint *breakpoint;
} LineInfo;

struct _IdeDebuggerBreakpoints
{
  GObject parent_instance;
  GArray *lines;
  GFile *file;
};

enum {
  PROP_0,
  PROP_FILE,
  N_PROPS
};

enum {
  CHANGED,
  N_SIGNALS
};

G_DEFINE_FINAL_TYPE (IdeDebuggerBreakpoints, ide_debugger_breakpoints, G_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
line_info_clear (gpointer data)
{
  LineInfo *info = data;

  info->line = 0;
  info->mode = 0;
  g_clear_object (&info->breakpoint);
}

static gint
line_info_compare (gconstpointer a,
                   gconstpointer b)
{
  const LineInfo *lia = a;
  const LineInfo *lib = b;

  return (gint)lia->line - (gint)lib->line;
}

static void
ide_debugger_breakpoints_dispose (GObject *object)
{
  IdeDebuggerBreakpoints *self = (IdeDebuggerBreakpoints *)object;

  g_clear_pointer (&self->lines, g_array_unref);

  G_OBJECT_CLASS (ide_debugger_breakpoints_parent_class)->dispose (object);
}

static void
ide_debugger_breakpoints_finalize (GObject *object)
{
  IdeDebuggerBreakpoints *self = (IdeDebuggerBreakpoints *)object;

  g_clear_object (&self->file);

  G_OBJECT_CLASS (ide_debugger_breakpoints_parent_class)->finalize (object);
}

static void
ide_debugger_breakpoints_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeDebuggerBreakpoints *self = IDE_DEBUGGER_BREAKPOINTS (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_breakpoints_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IdeDebuggerBreakpoints *self = IDE_DEBUGGER_BREAKPOINTS (object);

  switch (prop_id)
    {
    case PROP_FILE:
      self->file = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_debugger_breakpoints_class_init (IdeDebuggerBreakpointsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_debugger_breakpoints_dispose;
  object_class->finalize = ide_debugger_breakpoints_finalize;
  object_class->get_property = ide_debugger_breakpoints_get_property;
  object_class->set_property = ide_debugger_breakpoints_set_property;

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "The file for the breakpoints",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
ide_debugger_breakpoints_init (IdeDebuggerBreakpoints *self)
{
}

/**
 * ide_debugger_breakpoints_get_line:
 * @self: An #IdeDebuggerBreakpoints
 * @line: The line number
 *
 * Gets the breakpoint that has been registered at a given line, or %NULL
 * if no breakpoint is registered there.
 *
 * Returns: (nullable) (transfer none): An #IdeDebuggerBreakpoint or %NULL
 */
IdeDebuggerBreakpoint *
ide_debugger_breakpoints_get_line (IdeDebuggerBreakpoints *self,
                                   guint                   line)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER_BREAKPOINTS (self), NULL);

  if (self->lines != NULL)
    {
      LineInfo info = { line, 0 };
      LineInfo *ret;

      ret = bsearch (&info, (gpointer)self->lines->data,
                     self->lines->len, sizeof (LineInfo),
                     line_info_compare);

      if (ret)
        return ret->breakpoint;
    }

  return NULL;
}

IdeDebuggerBreakMode
ide_debugger_breakpoints_get_line_mode (IdeDebuggerBreakpoints *self,
                                        guint                   line)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER_BREAKPOINTS (self), 0);

  if (self->lines != NULL)
    {
      LineInfo info = { line, 0 };
      LineInfo *ret;

      ret = bsearch (&info, (gpointer)self->lines->data,
                     self->lines->len, sizeof (LineInfo),
                     line_info_compare);

      if (ret)
        return ret->mode;
    }

  return 0;
}

static void
ide_debugger_breakpoints_set_line (IdeDebuggerBreakpoints *self,
                                   guint                   line,
                                   IdeDebuggerBreakMode    mode,
                                   IdeDebuggerBreakpoint  *breakpoint)
{
  LineInfo info;

  g_assert (IDE_IS_DEBUGGER_BREAKPOINTS (self));
  g_assert (IDE_IS_DEBUGGER_BREAK_MODE (mode));
  g_assert (!breakpoint || IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (mode == IDE_DEBUGGER_BREAK_NONE || breakpoint != NULL);

  if (self->lines != NULL)
    {
      for (guint i = 0; i < self->lines->len; i++)
        {
          LineInfo *ele = &g_array_index (self->lines, LineInfo, i);

          if (ele->line == line)
            {
              if (mode != IDE_DEBUGGER_BREAK_NONE)
                {
                  ele->mode = mode;
                  g_set_object (&ele->breakpoint, breakpoint);
                }
              else
                g_array_remove_index (self->lines, i);

              goto emit_signal;
            }
        }
    }

  /* Nothing to do here */
  if (mode == IDE_DEBUGGER_BREAK_NONE)
    return;

  if (self->lines == NULL)
    {
      self->lines = g_array_new (FALSE, FALSE, sizeof (LineInfo));
      g_array_set_clear_func (self->lines, line_info_clear);
    }

  info.line = line;
  info.mode = mode;
  info.breakpoint = g_object_ref (breakpoint);

  g_array_append_val (self->lines, info);
  g_array_sort (self->lines, line_info_compare);

emit_signal:
  g_signal_emit (self, signals [CHANGED], 0);
}

void
_ide_debugger_breakpoints_add (IdeDebuggerBreakpoints *self,
                               IdeDebuggerBreakpoint  *breakpoint)
{
  IdeDebuggerBreakMode mode;
  guint line;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINTS (self));
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  line = ide_debugger_breakpoint_get_line (breakpoint);
  mode = ide_debugger_breakpoint_get_mode (breakpoint);

  IDE_TRACE_MSG ("tracking breakpoint at line %d [breakpoints=%p]",
                 line, self);

  ide_debugger_breakpoints_set_line (self, line, mode, breakpoint);

  IDE_EXIT;
}

void
_ide_debugger_breakpoints_remove (IdeDebuggerBreakpoints *self,
                                  IdeDebuggerBreakpoint  *breakpoint)
{
  guint line;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINTS (self));
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  line = ide_debugger_breakpoint_get_line (breakpoint);

  IDE_TRACE_MSG ("removing breakpoint at line %d [breakpoints=%p]",
                 line, self);

  if (self->lines != NULL)
    {
      /* First try to get things by pointer address to reduce chances
       * of removing the wrong breakpoint from the collection.
       */
      for (guint i = 0; i < self->lines->len; i++)
        {
          const LineInfo *info = &g_array_index (self->lines, LineInfo, i);

          if (ide_debugger_breakpoint_compare (breakpoint, info->breakpoint) == 0)
            {
              g_array_remove_index (self->lines, i);
              g_signal_emit (self, signals [CHANGED], 0);
              IDE_EXIT;
            }
        }

      ide_debugger_breakpoints_set_line (self, line, IDE_DEBUGGER_BREAK_NONE, NULL);
    }

  IDE_EXIT;
}

/**
 * ide_debugger_breakpoints_get_file:
 * @self: An #IdeDebuggerBreakpoints
 *
 * Gets the "file" property, which is the file that breakpoints within
 * this container belong to.
 *
 * Returns: (transfer none): a #GFile
 */
GFile *
ide_debugger_breakpoints_get_file (IdeDebuggerBreakpoints *self)
{
  g_return_val_if_fail (IDE_IS_DEBUGGER_BREAKPOINTS (self), NULL);

  return self->file;
}

/**
 * ide_debugger_breakpoints_foreach:
 * @self: a #IdeDebuggerBreakpoints
 * @func: (scope call) (closure user_data): a #GFunc to call
 * @user_data: user data for @func
 *
 * Call @func for every #IdeDebuggerBreakpoint in @self.
 */
void
ide_debugger_breakpoints_foreach (IdeDebuggerBreakpoints *self,
                                  GFunc                   func,
                                  gpointer                user_data)
{
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINTS (self));
  g_return_if_fail (func != NULL);

  if (self->lines != NULL)
    {
      for (guint i = 0; i < self->lines->len; i++)
        {
          const LineInfo *info = &g_array_index (self->lines, LineInfo, i);

          if (info->breakpoint)
            func (info->breakpoint, user_data);
        }
    }
}
