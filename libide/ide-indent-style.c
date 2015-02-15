/* ide-indent-style.c
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

#include "ide-indent-style.h"

GType
ide_indent_style_get_type (void)
{
  static gsize type_id;

  if (g_once_init_enter (&type_id))
    {
      gsize _type_id;
      static const GEnumValue values[] = {
        { IDE_INDENT_STYLE_SPACES, "IDE_INDENT_STYLE_SPACES", "SPACES" },
        { IDE_INDENT_STYLE_TABS, "IDE_INDENT_STYLE_TABS", "TABS" },
        { 0 }
      };

      _type_id = g_enum_register_static ("IdeIndentStyle", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
