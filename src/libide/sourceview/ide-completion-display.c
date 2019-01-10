/* ide-completion-display.c
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

#define G_LOG_DOMAIN "ide-completion-display"

#include "config.h"

#include "ide-completion-context.h"
#include "ide-completion-display.h"
#include "ide-completion-private.h"
#include "ide-source-view.h"

G_DEFINE_INTERFACE (IdeCompletionDisplay, ide_completion_display, GTK_TYPE_WIDGET)

static void
ide_completion_display_default_init (IdeCompletionDisplayInterface *iface)
{
}

void
ide_completion_display_set_context (IdeCompletionDisplay *self,
                                    IdeCompletionContext *context)
{
  g_return_if_fail (IDE_IS_COMPLETION_DISPLAY (self));
  g_return_if_fail (!context || IDE_IS_COMPLETION_CONTEXT (context));

  IDE_COMPLETION_DISPLAY_GET_IFACE (self)->set_context (self, context);
}

gboolean
ide_completion_display_key_press_event (IdeCompletionDisplay *self,
                                        const GdkEventKey    *key)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_DISPLAY (self), FALSE);
  g_return_val_if_fail (key!= NULL, FALSE);

  return IDE_COMPLETION_DISPLAY_GET_IFACE (self)->key_press_event (self, key);
}

void
ide_completion_display_set_n_rows (IdeCompletionDisplay *self,
                                   guint                 n_rows)
{
  g_return_if_fail (IDE_IS_COMPLETION_DISPLAY (self));
  g_return_if_fail (n_rows > 0);
  g_return_if_fail (n_rows <= 32);

  IDE_COMPLETION_DISPLAY_GET_IFACE (self)->set_n_rows (self, n_rows);
}

void
ide_completion_display_attach (IdeCompletionDisplay *self,
                               GtkSourceView        *view)
{
  g_return_if_fail (IDE_IS_COMPLETION_DISPLAY (self));
  g_return_if_fail (IDE_IS_SOURCE_VIEW (view));

  IDE_COMPLETION_DISPLAY_GET_IFACE (self)->attach (self, view);
}

void
ide_completion_display_move_cursor (IdeCompletionDisplay *self,
                                    GtkMovementStep       step,
                                    gint                  count)
{
  g_return_if_fail (IDE_IS_COMPLETION_DISPLAY (self));

  IDE_COMPLETION_DISPLAY_GET_IFACE (self)->move_cursor (self, step, count);
}

void
_ide_completion_display_set_font_desc (IdeCompletionDisplay       *self,
                                       const PangoFontDescription *font_desc)
{
  g_return_if_fail (IDE_IS_COMPLETION_DISPLAY (self));

  if (IDE_COMPLETION_DISPLAY_GET_IFACE (self)->set_font_desc)
    IDE_COMPLETION_DISPLAY_GET_IFACE (self)->set_font_desc (self, font_desc);
}
