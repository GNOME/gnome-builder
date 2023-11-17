/*
 * ide-terminal-palette.h
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

#pragma once

#include <gdk/gdk.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_TERMINAL_PALETTE (ide_terminal_palette_get_type())

/* Scarf is unused in Builder, but keepking it here makes it easier
 * to copy code between prompt and Builder.
 */
typedef struct _IdeTerminalPaletteScarf
{
  GdkRGBA foreground;
  GdkRGBA background;
} IdeTerminalPaletteScarf;

#define IDE_TERMINAL_PALETTE_SCARF_VISUAL_BELL 0
#define IDE_TERMINAL_PALETTE_SCARF_SUPERUSER   1
#define IDE_TERMINAL_PALETTE_SCARF_REMOTE      2
#define IDE_TERMINAL_PALETTE_N_SCARVES         3

typedef struct _IdeTerminalPaletteFace
{
  GdkRGBA background;
  GdkRGBA foreground;
  GdkRGBA cursor;
  GdkRGBA indexed[16];
  union {
    IdeTerminalPaletteScarf scarves[IDE_TERMINAL_PALETTE_N_SCARVES];
    struct {
      IdeTerminalPaletteScarf visual_bell;
      IdeTerminalPaletteScarf superuser;
      IdeTerminalPaletteScarf remote;
    };
  };
} IdeTerminalPaletteFace;

IDE_AVAILABLE_IN_46
G_DECLARE_FINAL_TYPE (IdeTerminalPalette, ide_terminal_palette, IDE, TERMINAL_PALETTE, GObject)

IDE_AVAILABLE_IN_46
GListModel                   *ide_terminal_palette_list_model_get_default (void);
IDE_AVAILABLE_IN_46
IdeTerminalPalette           *ide_terminal_palette_new_from_name          (const char    *name);
IDE_AVAILABLE_IN_46
const char                   *ide_terminal_palette_get_id                 (IdeTerminalPalette *self);
IDE_AVAILABLE_IN_46
const char                   *ide_terminal_palette_get_name               (IdeTerminalPalette *self);
IDE_AVAILABLE_IN_46
const IdeTerminalPaletteFace *ide_terminal_palette_get_face               (IdeTerminalPalette *self,
                                                                           gboolean       dark);

G_END_DECLS
