/* gb-command-vim-provider.c
 *
 * Copyright © 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gb-vim-command-provider"

#include <ide.h>

#include "gb-command-vim.h"
#include "gb-command-vim-provider.h"
#include "gb-vim.h"

struct _GbCommandVimProvider
{
  GbCommandProvider parent_instance;
};

G_DEFINE_TYPE (GbCommandVimProvider, gb_command_vim_provider, GB_TYPE_COMMAND_PROVIDER)

static GtkWidget *
get_active_widget (GbCommandProvider *provider)
{
  IdeWorkbench *workbench;
  IdeLayoutView *active_view;

  g_assert (GB_IS_COMMAND_VIM_PROVIDER (provider));

  /* Make sure we have a workbench */
  workbench = gb_command_provider_get_workbench (provider);
  if (!IDE_IS_WORKBENCH (workbench))
    return NULL;

  active_view = gb_command_provider_get_active_view (provider);
  if (active_view != NULL)
    return GTK_WIDGET (active_view);
  else
    return GTK_WIDGET (workbench);
}

static GbCommand *
gb_command_vim_provider_lookup (GbCommandProvider *provider,
                                const gchar       *command_text)
{
  GtkWidget *active_widget;

  g_return_val_if_fail (GB_IS_COMMAND_VIM_PROVIDER (provider), NULL);
  g_return_val_if_fail (command_text, NULL);

  active_widget = get_active_widget (provider);

  return g_object_new (GB_TYPE_COMMAND_VIM,
                       "command-text", command_text,
                       "active-widget", active_widget,
                       NULL);
}

static void
gb_command_vim_provider_complete (GbCommandProvider *provider,
                                  GPtrArray         *completions,
                                  const gchar       *initial_command_text)
{
  GtkWidget *active_widget;
  gchar **results;
  gsize i;

  g_return_if_fail (GB_IS_COMMAND_VIM_PROVIDER (provider));
  g_return_if_fail (completions);
  g_return_if_fail (initial_command_text);

  active_widget = get_active_widget (provider);

  results = gb_vim_complete (active_widget, initial_command_text);
  for (i = 0; results [i]; i++)
    g_ptr_array_add (completions, results [i]);
  g_free (results);
}

static void
gb_command_vim_provider_class_init (GbCommandVimProviderClass *klass)
{
  GbCommandProviderClass *provider_class = GB_COMMAND_PROVIDER_CLASS (klass);

  provider_class->lookup = gb_command_vim_provider_lookup;
  provider_class->complete = gb_command_vim_provider_complete;
}

static void
gb_command_vim_provider_init (GbCommandVimProvider *self)
{
}
