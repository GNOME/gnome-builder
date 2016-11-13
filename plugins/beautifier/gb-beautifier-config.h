/* gb-beautifier-config.h
 *
 * Copyright (C) 2016 sebastien lafargue <slafargue@gnome.org>
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

#ifndef GB_BEAUTIFIER_CONFIG_H
#define GB_BEAUTIFIER_CONFIG_H

#include <glib.h>

#include "gb-beautifier-workbench-addin.h"

G_BEGIN_DECLS

typedef enum
{
  GB_BEAUTIFIER_CONFIG_COMMAND_UNCRUSTIFY,
} GbBeautifierConfigCommand;

typedef struct
{
  gchar                     *lang_id;
  GFile                     *file;
  gchar                     *name;
  GbBeautifierConfigCommand  command;
  guint                      is_default : 1;
} GbBeautifierConfigEntry;

typedef struct
{
  gchar *lang_id;
  gchar *profile;
  gchar *default_profile;
} GbBeautifierMapEntry;

GArray *gb_beautifier_config_get_entries (GbBeautifierWorkbenchAddin *self);

G_END_DECLS

#endif /* GB_BEAUTIFIER_CONFIG_H */
