/* ide-terminal-palettes-inline.h
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

#include "ide-terminal-palette.h"

G_BEGIN_DECLS

#define _GDK_RGBA_DECODE(c) ((unsigned)(((c) >= 'A' && (c) <= 'F') ? ((c)-'A'+10) : \
                                        ((c) >= 'a' && (c) <= 'f') ? ((c)-'a'+10) : \
                                        ((c) >= '0' && (c) <= '9') ? ((c)-'0') : \
                                        -1))
#define _GDK_RGBA_SELECT_COLOR(_str, index3, index6) (sizeof(_str) <= 4 ? _GDK_RGBA_DECODE ((_str)[index3]) : _GDK_RGBA_DECODE ((_str)[index6]))
#define GDK_RGBA(str) ((GdkRGBA) {\
    ((_GDK_RGBA_SELECT_COLOR(str, 0, 0) << 4) | _GDK_RGBA_SELECT_COLOR(str, 0, 1)) / 255., \
    ((_GDK_RGBA_SELECT_COLOR(str, 1, 2) << 4) | _GDK_RGBA_SELECT_COLOR(str, 1, 3)) / 255., \
    ((_GDK_RGBA_SELECT_COLOR(str, 2, 4) << 4) | _GDK_RGBA_SELECT_COLOR(str, 2, 5)) / 255., \
    ((sizeof(str) % 4 == 1) ? ((_GDK_RGBA_SELECT_COLOR(str, 3, 6) << 4) | _GDK_RGBA_SELECT_COLOR(str, 3, 7)) : 0xFF) / 255. })

typedef struct _IdeTerminalPaletteData
{
  const char             *id;
  const char             *name;
  IdeTerminalPaletteFace  faces[2];
} IdeTerminalPaletteData;

static const IdeTerminalPaletteData ide_terminal_palettes_inline[] = {
  {
    .id = "gnome",
    .name = N_("GNOME"),
    .faces = {
      {
        .foreground = GDK_RGBA ("1e1e1e"),
        .background = GDK_RGBA ("ffffff"),
        .indexed = {
          GDK_RGBA ("1e1e1e"),
          GDK_RGBA ("c01c28"),
          GDK_RGBA ("26a269"),
          GDK_RGBA ("a2734c"),
          GDK_RGBA ("12488b"),
          GDK_RGBA ("a347ba"),
          GDK_RGBA ("2aa1b3"),
          GDK_RGBA ("d0cfcc"),
          GDK_RGBA ("5e5c64"),
          GDK_RGBA ("f66151"),
          GDK_RGBA ("33d17a"),
          GDK_RGBA ("e9ad0c"),
          GDK_RGBA ("2a7bde"),
          GDK_RGBA ("c061cb"),
          GDK_RGBA ("33c7de"),
          GDK_RGBA ("ffffff"),
        },
      },
      {
        .foreground = GDK_RGBA ("ffffff"),
        .background = GDK_RGBA ("1e1e1e"),
        .indexed = {
          GDK_RGBA ("1e1e1e"),
          GDK_RGBA ("c01c28"),
          GDK_RGBA ("26a269"),
          GDK_RGBA ("a2734c"),
          GDK_RGBA ("12488b"),
          GDK_RGBA ("a347ba"),
          GDK_RGBA ("2aa1b3"),
          GDK_RGBA ("d0cfcc"),
          GDK_RGBA ("5e5c64"),
          GDK_RGBA ("f66151"),
          GDK_RGBA ("33d17a"),
          GDK_RGBA ("e9ad0c"),
          GDK_RGBA ("2a7bde"),
          GDK_RGBA ("c061cb"),
          GDK_RGBA ("33c7de"),
          GDK_RGBA ("ffffff"),
        },
      },
    },
  },

  {
    .id = "solarized",
    .name = N_("Solarized"),
    .faces = {
      {
        .foreground = GDK_RGBA ("002b36"),
        .background = GDK_RGBA ("fdf6e3"),
        .cursor = GDK_RGBA ("93a1a1"),
        .indexed = {
          GDK_RGBA ("073642"),
          GDK_RGBA ("dc322f"),
          GDK_RGBA ("859900"),
          GDK_RGBA ("b58900"),
          GDK_RGBA ("268ad2"),
          GDK_RGBA ("d33682"),
          GDK_RGBA ("2aa198"),
          GDK_RGBA ("eee8d5"),
          GDK_RGBA ("002b36"),
          GDK_RGBA ("cb4b16"),
          GDK_RGBA ("657b83"),
          GDK_RGBA ("586e75"),
          GDK_RGBA ("93a1a1"),
          GDK_RGBA ("6c71c4"),
          GDK_RGBA ("839496"),
          GDK_RGBA ("fdf6e3"),
        },
      },
      {
        .foreground = GDK_RGBA ("839496"),
        .background = GDK_RGBA ("002b36"),
        .cursor = GDK_RGBA ("93a1a1"),
        .indexed = {
          GDK_RGBA ("073642"),
          GDK_RGBA ("dc322f"),
          GDK_RGBA ("859900"),
          GDK_RGBA ("b58900"),
          GDK_RGBA ("268bd2"),
          GDK_RGBA ("d33682"),
          GDK_RGBA ("2aa198"),
          GDK_RGBA ("eee8d5"),
          GDK_RGBA ("002b36"),
          GDK_RGBA ("cb4b16"),
          GDK_RGBA ("586e75"),
          GDK_RGBA ("657b83"),
          GDK_RGBA ("839496"),
          GDK_RGBA ("6c71c4"),
          GDK_RGBA ("93a1a1"),
          GDK_RGBA ("fdf6e3"),
        },
      },
    },
  },

  {
    .id = "tango",
    .name = N_("Tango"),
    .faces = {
      {
        .foreground = GDK_RGBA ("2e3436"),
        .background = GDK_RGBA ("eeeeec"),
        .indexed = {
          GDK_RGBA ("2e3436"),
          GDK_RGBA ("cc0000"),
          GDK_RGBA ("4e9a06"),
          GDK_RGBA ("c4a000"),
          GDK_RGBA ("3465a4"),
          GDK_RGBA ("75507b"),
          GDK_RGBA ("06989a"),
          GDK_RGBA ("d3d7cf"),
          GDK_RGBA ("555753"),
          GDK_RGBA ("ef2929"),
          GDK_RGBA ("8ae234"),
          GDK_RGBA ("fce94f"),
          GDK_RGBA ("729fcf"),
          GDK_RGBA ("ad7fa8"),
          GDK_RGBA ("34e2e2"),
          GDK_RGBA ("eeeeec"),
        },
      },
      {
        .foreground = GDK_RGBA ("d3d7cf"),
        .background = GDK_RGBA ("2e3436"),
        .indexed = {
          GDK_RGBA ("2e3436"),
          GDK_RGBA ("cc0000"),
          GDK_RGBA ("4e9a06"),
          GDK_RGBA ("c4a000"),
          GDK_RGBA ("3465a4"),
          GDK_RGBA ("75507b"),
          GDK_RGBA ("06989a"),
          GDK_RGBA ("d3d7cf"),
          GDK_RGBA ("555753"),
          GDK_RGBA ("ef2929"),
          GDK_RGBA ("8ae234"),
          GDK_RGBA ("fce94f"),
          GDK_RGBA ("729fcf"),
          GDK_RGBA ("ad7fa8"),
          GDK_RGBA ("34e2e2"),
          GDK_RGBA ("eeeeec"),
        },
      },
    },
  },

  {
    .id = "dracula",
    .name = N_("Dracula"),
    .faces = {
      {
        .foreground = GDK_RGBA ("282A36"),
        .background = GDK_RGBA ("ffffff"),
        .indexed = {
          GDK_RGBA ("f1f2ff"),
          GDK_RGBA ("b60021"),
          GDK_RGBA ("006800"),
          GDK_RGBA ("515f00"),
          GDK_RGBA ("6946a3"),
          GDK_RGBA ("a41d74"),
          GDK_RGBA ("006274"),
          GDK_RGBA ("f8f8f2"),
          GDK_RGBA ("8393c7"),
          GDK_RGBA ("ac202f"),
          GDK_RGBA ("006803"),
          GDK_RGBA ("585e06"),
          GDK_RGBA ("6c4993"),
          GDK_RGBA ("962f7c"),
          GDK_RGBA ("006465"),
          GDK_RGBA ("595959"),
        },
      },
      {
        .foreground = GDK_RGBA ("f8f8f2"),
        .background = GDK_RGBA ("282A36"),
        .indexed = {
          GDK_RGBA ("21222c"),
          GDK_RGBA ("ff5555"),
          GDK_RGBA ("50fa7b"),
          GDK_RGBA ("f1fa8c"),
          GDK_RGBA ("bd93f9"),
          GDK_RGBA ("ff79c6"),
          GDK_RGBA ("8be9fd"),
          GDK_RGBA ("f8f8f2"),
          GDK_RGBA ("6272a4"),
          GDK_RGBA ("ff6e6e"),
          GDK_RGBA ("69ff94"),
          GDK_RGBA ("ffffa5"),
          GDK_RGBA ("d6acff"),
          GDK_RGBA ("ff92df"),
          GDK_RGBA ("a4ffff"),
          GDK_RGBA ("ffffff"),
        },
      },
    },
  },

  {
    .id = "nord",
    .name = N_("Nord"),
    .faces = {
      {
        .foreground = GDK_RGBA ("414858"),
        .background = GDK_RGBA ("e5e9f0"),
        .indexed = {
          GDK_RGBA ("3b4251"),
          GDK_RGBA ("bf6069"),
          GDK_RGBA ("a3be8b"),
          GDK_RGBA ("eacb8a"),
          GDK_RGBA ("81a1c1"),
          GDK_RGBA ("b48dac"),
          GDK_RGBA ("88c0d0"),
          GDK_RGBA ("d8dee9"),
          GDK_RGBA ("4c556a"),
          GDK_RGBA ("bf6069"),
          GDK_RGBA ("a3be8b"),
          GDK_RGBA ("eacb8a"),
          GDK_RGBA ("81a1c1"),
          GDK_RGBA ("b48dac"),
          GDK_RGBA ("8fbcbb"),
          GDK_RGBA ("eceff4"),
        },
      },
      {
        .foreground = GDK_RGBA ("d8dee9"),
        .background = GDK_RGBA ("2e3440"),
        .indexed = {
          GDK_RGBA ("3b4252"),
          GDK_RGBA ("bf616a"),
          GDK_RGBA ("a3be8c"),
          GDK_RGBA ("ebcb8b"),
          GDK_RGBA ("81a1c1"),
          GDK_RGBA ("b48ead"),
          GDK_RGBA ("88c0d0"),
          GDK_RGBA ("e5e9f0"),
          GDK_RGBA ("4c566a"),
          GDK_RGBA ("bf616a"),
          GDK_RGBA ("a3be8c"),
          GDK_RGBA ("ebcb8b"),
          GDK_RGBA ("81a1c1"),
          GDK_RGBA ("b48ead"),
          GDK_RGBA ("8fbcbb"),
          GDK_RGBA ("eceff4"),
        },
      },
    },
  },

  {
    .id = "linux",
    .name = N_("Linux"),
    .faces = {
      {
        .foreground = GDK_RGBA ("000000"),
        .background = GDK_RGBA ("ffffff"),
        .indexed = {
          GDK_RGBA ("000000"),
          GDK_RGBA ("aa0000"),
          GDK_RGBA ("00aa00"),
          GDK_RGBA ("aa5500"),
          GDK_RGBA ("0000aa"),
          GDK_RGBA ("aa00aa"),
          GDK_RGBA ("00aaaa"),
          GDK_RGBA ("aaaaaa"),
          GDK_RGBA ("555555"),
          GDK_RGBA ("ff5555"),
          GDK_RGBA ("55ff55"),
          GDK_RGBA ("ffff55"),
          GDK_RGBA ("5555ff"),
          GDK_RGBA ("ff55ff"),
          GDK_RGBA ("55ffff"),
          GDK_RGBA ("ffffff"),
        },
      },
      {
        .foreground = GDK_RGBA ("ffffff"),
        .background = GDK_RGBA ("000000"),
        .indexed = {
          GDK_RGBA ("000000"),
          GDK_RGBA ("aa0000"),
          GDK_RGBA ("00aa00"),
          GDK_RGBA ("aa5500"),
          GDK_RGBA ("0000aa"),
          GDK_RGBA ("aa00aa"),
          GDK_RGBA ("00aaaa"),
          GDK_RGBA ("aaaaaa"),
          GDK_RGBA ("555555"),
          GDK_RGBA ("ff5555"),
          GDK_RGBA ("55ff55"),
          GDK_RGBA ("ffff55"),
          GDK_RGBA ("5555ff"),
          GDK_RGBA ("ff55ff"),
          GDK_RGBA ("55ffff"),
          GDK_RGBA ("ffffff"),
        },
      },
    },
  },

  {
    .id = "xterm",
    .name = N_("XTerm"),
    .faces = {
      {
        .foreground = GDK_RGBA ("000000"),
        .background = GDK_RGBA ("ffffff"),
        .indexed = {
          GDK_RGBA ("000000"),
          GDK_RGBA ("cd0000"),
          GDK_RGBA ("00cd00"),
          GDK_RGBA ("cdcd00"),
          GDK_RGBA ("0000ee"),
          GDK_RGBA ("cd00cd"),
          GDK_RGBA ("00cdcd"),
          GDK_RGBA ("e5e5e5"),
          GDK_RGBA ("7f7f7f"),
          GDK_RGBA ("ff0000"),
          GDK_RGBA ("00ff00"),
          GDK_RGBA ("ffff00"),
          GDK_RGBA ("5c5cff"),
          GDK_RGBA ("ff00ff"),
          GDK_RGBA ("00ffff"),
          GDK_RGBA ("ffffff"),
        },
      },
      {
        .foreground = GDK_RGBA ("ffffff"),
        .background = GDK_RGBA ("000000"),
        .indexed = {
          GDK_RGBA ("000000"),
          GDK_RGBA ("cd0000"),
          GDK_RGBA ("00cd00"),
          GDK_RGBA ("cdcd00"),
          GDK_RGBA ("0000ee"),
          GDK_RGBA ("cd00cd"),
          GDK_RGBA ("00cdcd"),
          GDK_RGBA ("e5e5e5"),
          GDK_RGBA ("7f7f7f"),
          GDK_RGBA ("ff0000"),
          GDK_RGBA ("00ff00"),
          GDK_RGBA ("ffff00"),
          GDK_RGBA ("5c5cff"),
          GDK_RGBA ("ff00ff"),
          GDK_RGBA ("00ffff"),
          GDK_RGBA ("ffffff"),
        },
      },
    },
  },

  {
    .id = "rxvt",
    .name = N_("RXVT"),
    .faces = {
      {
        .foreground = GDK_RGBA ("000000"),
        .background = GDK_RGBA ("ffffff"),
        .indexed = {
          GDK_RGBA ("000000"),
          GDK_RGBA ("cd0000"),
          GDK_RGBA ("00cd00"),
          GDK_RGBA ("cdcd00"),
          GDK_RGBA ("0000cd"),
          GDK_RGBA ("cd00cd"),
          GDK_RGBA ("00cdcd"),
          GDK_RGBA ("faebd7"),
          GDK_RGBA ("404040"),
          GDK_RGBA ("ff0000"),
          GDK_RGBA ("00ff00"),
          GDK_RGBA ("ffff00"),
          GDK_RGBA ("0000ff"),
          GDK_RGBA ("ff00ff"),
          GDK_RGBA ("00ffff"),
          GDK_RGBA ("ffffff"),
        },
      },
      {
        .foreground = GDK_RGBA ("ffffff"),
        .background = GDK_RGBA ("000000"),
        .indexed = {
          GDK_RGBA ("000000"),
          GDK_RGBA ("cd0000"),
          GDK_RGBA ("00cd00"),
          GDK_RGBA ("cdcd00"),
          GDK_RGBA ("0000cd"),
          GDK_RGBA ("cd00cd"),
          GDK_RGBA ("00cdcd"),
          GDK_RGBA ("faebd7"),
          GDK_RGBA ("404040"),
          GDK_RGBA ("ff0000"),
          GDK_RGBA ("00ff00"),
          GDK_RGBA ("ffff00"),
          GDK_RGBA ("0000ff"),
          GDK_RGBA ("ff00ff"),
          GDK_RGBA ("00ffff"),
          GDK_RGBA ("ffffff"),
        },
      },
    },
  },

  {
    .id = "fishtank",
    .name = N_("Fishtank"),
    .faces = {
      {
        .foreground = GDK_RGBA ("ECF0FE"),
        .background = GDK_RGBA ("232537"),
        .cursor = GDK_RGBA ("ECF0FE"),
        .indexed = {
          GDK_RGBA ("03073C"),
          GDK_RGBA ("C6004A"),
          GDK_RGBA ("ACF157"),
          GDK_RGBA ("FECD5E"),
          GDK_RGBA ("525FB8"),
          GDK_RGBA ("986F82"),
          GDK_RGBA ("968763"),
          GDK_RGBA ("ECF0FC"),
          GDK_RGBA ("6C5B30"),
          GDK_RGBA ("DA4B8A"),
          GDK_RGBA ("DBFFA9"),
          GDK_RGBA ("FEE6A9"),
          GDK_RGBA ("B2BEFA"),
          GDK_RGBA ("FDA5CD"),
          GDK_RGBA ("A5BD86"),
          GDK_RGBA ("F6FFEC"),
        },
      },
      {
        .foreground = GDK_RGBA ("ECF0FE"),
        .background = GDK_RGBA ("232537"),
        .cursor = GDK_RGBA ("ECF0FE"),
        .indexed = {
          GDK_RGBA ("03073C"),
          GDK_RGBA ("C6004A"),
          GDK_RGBA ("ACF157"),
          GDK_RGBA ("FECD5E"),
          GDK_RGBA ("525FB8"),
          GDK_RGBA ("986F82"),
          GDK_RGBA ("968763"),
          GDK_RGBA ("ECF0FC"),
          GDK_RGBA ("6C5B30"),
          GDK_RGBA ("DA4B8A"),
          GDK_RGBA ("DBFFA9"),
          GDK_RGBA ("FEE6A9"),
          GDK_RGBA ("B2BEFA"),
          GDK_RGBA ("FDA5CD"),
          GDK_RGBA ("A5BD86"),
          GDK_RGBA ("F6FFEC"),
        },
      },
    },
  },
};

G_END_DECLS
