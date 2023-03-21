/* ide-buffer-addin-private.h
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

#pragma once

#include <libide-plugins.h>
#include <libpeas.h>

#include "ide-buffer.h"
#include "ide-buffer-addin.h"

G_BEGIN_DECLS

typedef struct
{
  IdeBuffer   *buffer;
  const gchar *language_id;
} IdeBufferLanguageSet;

typedef struct
{
  IdeBuffer *buffer;
  GFile     *file;
} IdeBufferFileSave;

typedef struct
{
  IdeBuffer *buffer;
  GFile     *file;
} IdeBufferFileLoad;

void _ide_buffer_addin_load_cb                 (IdeExtensionSetAdapter *set,
                                                PeasPluginInfo         *plugin_info,
                                                GObject          *exten,
                                                gpointer                user_data);
void _ide_buffer_addin_unload_cb               (IdeExtensionSetAdapter *set,
                                                PeasPluginInfo         *plugin_info,
                                                GObject          *exten,
                                                gpointer                user_data);
void _ide_buffer_addin_file_loaded_cb          (IdeExtensionSetAdapter *set,
                                                PeasPluginInfo         *plugin_info,
                                                GObject          *exten,
                                                gpointer                user_data);
void _ide_buffer_addin_save_file_cb            (IdeExtensionSetAdapter *set,
                                                PeasPluginInfo         *plugin_info,
                                                GObject          *exten,
                                                gpointer                user_data);
void _ide_buffer_addin_file_saved_cb           (IdeExtensionSetAdapter *set,
                                                PeasPluginInfo         *plugin_info,
                                                GObject          *exten,
                                                gpointer                user_data);
void _ide_buffer_addin_language_set_cb         (IdeExtensionSetAdapter *set,
                                                PeasPluginInfo         *plugin_info,
                                                GObject          *exten,
                                                gpointer                user_data);
void _ide_buffer_addin_change_settled_cb       (IdeExtensionSetAdapter *set,
                                                PeasPluginInfo         *plugin_info,
                                                GObject          *exten,
                                                gpointer                user_data);
void _ide_buffer_addin_style_scheme_changed_cb (IdeExtensionSetAdapter *set,
                                                PeasPluginInfo         *plugin_info,
                                                GObject          *exten,
                                                gpointer                user_data);

G_END_DECLS
