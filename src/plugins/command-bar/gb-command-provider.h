/* gb-command-provider.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#pragma once

#include <gio/gio.h>
#include <ide.h>

#include "gb-command.h"

G_BEGIN_DECLS

#define GB_TYPE_COMMAND_PROVIDER (gb_command_provider_get_type())

G_DECLARE_DERIVABLE_TYPE (GbCommandProvider, gb_command_provider, GB, COMMAND_PROVIDER, GObject)

struct _GbCommandProviderClass
{
  GObjectClass parent;

  GbCommand *(*lookup)   (GbCommandProvider *provider,
                          const gchar       *command_text);
  void       (*complete) (GbCommandProvider *provider,
                          GPtrArray         *completions,
                          const gchar       *command_text);
};

GbCommandProvider *gb_command_provider_new             (IdeWorkbench      *workbench);
IdeWorkbench      *gb_command_provider_get_workbench   (GbCommandProvider *provider);
IdeLayoutView     *gb_command_provider_get_active_view (GbCommandProvider *provider);
gint               gb_command_provider_get_priority    (GbCommandProvider *provider);
void               gb_command_provider_set_priority    (GbCommandProvider *provider,
                                                        gint               priority);
GbCommand         *gb_command_provider_lookup          (GbCommandProvider *provider,
                                                        const gchar       *command_text);
void               gb_command_provider_complete        (GbCommandProvider *provider,
                                                        GPtrArray         *completions,
                                                        const gchar       *initial_command_text);

G_END_DECLS
