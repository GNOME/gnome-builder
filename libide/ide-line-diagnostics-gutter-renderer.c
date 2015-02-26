/* ide-line-diagnostics-gutter-renderer.c
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

#include "ide-buffer.h"
#include "ide-line-diagnostics-gutter-renderer.h"

G_DEFINE_TYPE (IdeLineDiagnosticsGutterRenderer,
               ide_line_diagnostics_gutter_renderer,
               GTK_SOURCE_TYPE_GUTTER_RENDERER_PIXBUF)

static void
ide_line_diagnostics_gutter_renderer_query_data (GtkSourceGutterRenderer      *renderer,
                                                 GtkTextIter                  *begin,
                                                 GtkTextIter                  *end,
                                                 GtkSourceGutterRendererState  state)
{
  GtkTextBuffer *buffer;
  IdeBufferLineFlags flags;
  const gchar *icon_name = NULL;
  guint line;

  g_return_if_fail (IDE_IS_LINE_DIAGNOSTICS_GUTTER_RENDERER (renderer));
  g_return_if_fail (begin);
  g_return_if_fail (end);

  buffer = gtk_text_iter_get_buffer (begin);

  if (!IDE_IS_BUFFER (buffer))
    return;

  line = gtk_text_iter_get_line (begin);
  flags = ide_buffer_get_line_flags (IDE_BUFFER (buffer), line);
  flags &= IDE_BUFFER_LINE_FLAGS_DIAGNOSTICS_MASK;

  if (flags == 0)
    icon_name = NULL;
  else if ((flags & IDE_BUFFER_LINE_FLAGS_ERROR) != 0)
    icon_name = "process-stop-symbolic";
  else if ((flags & IDE_BUFFER_LINE_FLAGS_WARNING) != 0)
    icon_name = "dialog-warning-symbolic";
  else if ((flags & IDE_BUFFER_LINE_FLAGS_NOTE) != 0)
    icon_name = "dialog-information-symbolic";
  else
    icon_name = NULL;

  if (icon_name)
    g_object_set (renderer, "icon-name", icon_name, NULL);
  else
    g_object_set (renderer, "pixbuf", NULL, NULL);
}

static void
ide_line_diagnostics_gutter_renderer_class_init (IdeLineDiagnosticsGutterRendererClass *klass)
{
  GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);

  renderer_class->query_data = ide_line_diagnostics_gutter_renderer_query_data;
}

static void
ide_line_diagnostics_gutter_renderer_init (IdeLineDiagnosticsGutterRenderer *self)
{
}
