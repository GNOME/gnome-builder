/* gb-beautifier-config.h
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#include <glib.h>

#include "gb-beautifier-editor-addin.h"

G_BEGIN_DECLS

typedef enum
{
  GB_BEAUTIFIER_CONFIG_COMMAND_NONE,
  GB_BEAUTIFIER_CONFIG_COMMAND_CLANG_FORMAT
} GbBeautifierConfigCommand;

typedef struct
{
  gchar                     *lang_id;
  GFile                     *config_file;
  gchar                     *name;
  GbBeautifierConfigCommand  command;
  GArray                    *command_args;
  guint                      is_default : 1;
  guint                      is_config_file_temp : 1;
} GbBeautifierConfigEntry;

typedef struct
{
  gchar *str;
  guint  is_path : 1;
  guint  is_temp : 1;
} GbBeautifierCommandArg;

typedef struct
{
  gchar *lang_id;
  gchar *mapped_lang_id;
  gchar *default_profile;
} GbBeautifierMapEntry;

typedef struct
{
  GArray   *entries;
  gboolean  has_default;
} GbBeautifierEntriesResult;

void                            gb_beautifier_config_get_entries_async       (GbBeautifierEditorAddin  *self,
                                                                              gboolean                 *has_default,
                                                                              GAsyncReadyCallback       callback,
                                                                              GCancellable             *cancellable,
                                                                              gpointer                  user_data);
GbBeautifierEntriesResult      *gb_beautifier_config_get_entries_finish      (GbBeautifierEditorAddin  *self,
                                                                              GAsyncResult             *result,
                                                                              GError                  **error);

void                            gb_beautifier_entries_result_free            (gpointer                  data);

G_END_DECLS
