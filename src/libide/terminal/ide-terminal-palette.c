/*
 * ide-terminal-palette.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <glib/gi18n.h>

#include "ide-terminal-palette.h"
#include "ide-terminal-palettes.h"

/* If you're here, you might be wondering if there is support for
 * custom installation of palettes. Currently, the answer is no. But
 * if you were going to venture on that journey, here is how you
 * should implement it.
 *
 *  0) Add a deserialize from GFile/GKeyFile constructor
 *  1) Add a GListModel to PromptApplication to hold dyanmically
 *     loaded palettes.
 *  2) Drop palettes in something like .local/share/appname/palettes/
 *  3) The format for palettes could probably just be GKeyFile with
 *     key/value pairs for everything we have in static data. I'm
 *     sure there is another GTK based terminal which already has a
 *     reasonable palette definition like this you can borrow.
 *  4) Use a GtkFlattenListModel to join our internal and dynamic
 *     palettes together.
 *  5) Add loader to PromptApplication at startup. It's fine to
 *     just require reloading of the app to pick them up, but a
 *     GFileMonitor might be nice.
 */


struct _IdeTerminalPalette
{
  GObject parent_instance;
  const IdeTerminalPaletteData *palette;
};

G_DEFINE_FINAL_TYPE (IdeTerminalPalette, ide_terminal_palette, G_TYPE_OBJECT)

static void
ide_terminal_palette_class_init (IdeTerminalPaletteClass *klass)
{
}

static void
ide_terminal_palette_init (IdeTerminalPalette *self)
{
}

IdeTerminalPalette *
ide_terminal_palette_new_from_name (const char *name)
{
  const IdeTerminalPaletteData *data = NULL;
  IdeTerminalPalette *self;

  for (guint i = 0; i < G_N_ELEMENTS (ide_terminal_palettes_inline); i++)
    {
      if (g_strcmp0 (name, ide_terminal_palettes_inline[i].id) == 0)
        {
          data = &ide_terminal_palettes_inline[i];
          break;
        }
    }

  if (data == NULL)
    data = &ide_terminal_palettes_inline[0];

  self = g_object_new (IDE_TYPE_TERMINAL_PALETTE, NULL);
  self->palette = data;

  return self;
}

const char *
ide_terminal_palette_get_id (IdeTerminalPalette *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_PALETTE (self), NULL);

  return self->palette->id;
}

const char *
ide_terminal_palette_get_name (IdeTerminalPalette *self)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_PALETTE (self), NULL);

  return self->palette->name;
}

const IdeTerminalPaletteFace *
ide_terminal_palette_get_face (IdeTerminalPalette *self,
                               gboolean       dark)
{
  g_return_val_if_fail (IDE_IS_TERMINAL_PALETTE (self), NULL);

  return &self->palette->faces[!!dark];
}

/**
 * ide_terminal_palette_list_model_get_default:
 *
 * Returns: (transfer none) (not nullable): a #GListModel of #IdeTerminalPalette
 */
GListModel *
ide_terminal_palette_list_model_get_default (void)
{
  static GListStore *instance;

  if (instance == NULL)
    {
      instance = g_list_store_new (IDE_TYPE_TERMINAL_PALETTE);
      g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *)&instance);

      for (guint i = 0; i < G_N_ELEMENTS (ide_terminal_palettes_inline); i++)
        {
          g_autoptr(IdeTerminalPalette) palette = ide_terminal_palette_new_from_name (ide_terminal_palettes_inline[i].id);
          g_list_store_append (instance, palette);
        }
    }

  return G_LIST_MODEL (instance);
}
