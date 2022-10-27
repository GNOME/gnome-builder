/* ide-lsp-plugin-private.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

#include "ide-lsp-plugin.h"

G_BEGIN_DECLS

typedef struct _IdeLspPluginInfo
{
  char *module_name;
  char **command;
  char **languages;
  GBytes *default_settings;
  GType service_type;
  GType completion_provider_type;
  GType code_action_provider_type;
  GType diagnostic_provider_type;
  GType formatter_type;
  GType highlighter_type;
  GType hover_provider_type;
  GType rename_provider_type;
  GType search_provider_type;
  GType symbol_resolver_type;
} IdeLspPluginInfo;

IdeLspPluginInfo *ide_lsp_plugin_info_ref                   (IdeLspPluginInfo *info);
void              ide_lsp_plugin_info_unref                 (IdeLspPluginInfo *info);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
void              ide_lsp_plugin_remove_plugin_info_param    (guint            *n_parameters,
                                                              GParameter       *parameters);
GObject          *ide_lsp_plugin_create_code_action_provider (guint             n_parameters,
                                                              GParameter       *parameters,
                                                              IdeLspPluginInfo *info);
GObject          *ide_lsp_plugin_create_completion_provider  (guint             n_parameters,
                                                              GParameter       *parameters,
                                                              IdeLspPluginInfo *info);
GObject          *ide_lsp_plugin_create_diagnostic_provider  (guint             n_parameters,
                                                              GParameter       *parameters,
                                                              IdeLspPluginInfo *info);
GObject          *ide_lsp_plugin_create_formatter            (guint             n_parameters,
                                                              GParameter       *parameters,
                                                              IdeLspPluginInfo *info);
GObject          *ide_lsp_plugin_create_highlighter          (guint             n_parameters,
                                                              GParameter       *parameters,
                                                              IdeLspPluginInfo *info);
GObject          *ide_lsp_plugin_create_hover_provider       (guint             n_parameters,
                                                              GParameter       *parameters,
                                                              IdeLspPluginInfo *info);
GObject          *ide_lsp_plugin_create_rename_provider      (guint             n_parameters,
                                                              GParameter       *parameters,
                                                              IdeLspPluginInfo *info);
GObject          *ide_lsp_plugin_create_search_provider      (guint             n_parameters,
                                                              GParameter       *parameters,
                                                              IdeLspPluginInfo *info);
GObject          *ide_lsp_plugin_create_symbol_resolver      (guint             n_parameters,
                                                              GParameter       *parameters,
                                                              IdeLspPluginInfo *info);
G_GNUC_END_IGNORE_DEPRECATIONS

G_END_DECLS
