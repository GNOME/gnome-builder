/* gb-command-vim-provider.c
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

#define G_LOG_DOMAIN "gb-vim-command-provider"

#include <ide.h>

#include "gb-command-vim.h"
#include "gb-command-vim-provider.h"
#include "gb-editor-frame.h"
#include "gb-editor-frame-private.h"
#include "gb-editor-view.h"
#include "gb-editor-view-private.h"
#include "gb-vim.h"
#include "gb-workbench.h"

struct _GbCommandVimProvider
{
  GbCommandProvider parent_instance;
};

G_DEFINE_TYPE (GbCommandVimProvider, gb_command_vim_provider, GB_TYPE_COMMAND_PROVIDER)

GtkWidget *
get_source_view (GbCommandProvider *provider)
{
  GbWorkbench *workbench;
  GbView *active_view;
  IdeSourceView *source_view;

  g_assert (GB_IS_COMMAND_VIM_PROVIDER (provider));

  /* Make sure we have a workbench */
  workbench = gb_command_provider_get_workbench (provider);
  if (!GB_IS_WORKBENCH (workbench))
    return NULL;

  /* Make sure we have an editor tab last focused */
  active_view = gb_command_provider_get_active_view (provider);
  if (!GB_IS_EDITOR_VIEW (active_view))
    return NULL;

  /* TODO: Perhaps get the last focused frame? */
  source_view = GB_EDITOR_VIEW (active_view)->frame1->source_view;
  if (!IDE_IS_SOURCE_VIEW (source_view))
    return NULL;

  return GTK_WIDGET (source_view);
}

static GbCommand *
gb_command_vim_provider_lookup (GbCommandProvider *provider,
                                const gchar       *command_text)
{
  GtkWidget *source_view;

  g_return_val_if_fail (GB_IS_COMMAND_VIM_PROVIDER (provider), NULL);
  g_return_val_if_fail (command_text, NULL);

  source_view = get_source_view (provider);

  return g_object_new (GB_TYPE_COMMAND_VIM,
                       "command-text", command_text,
                       "source-view", source_view,
                       NULL);
}

static void
gb_command_vim_provider_complete (GbCommandProvider *provider,
                                  GPtrArray         *completions,
                                  const gchar       *initial_command_text)
{
  GtkWidget *source_view;
  gchar **results;
  gsize i;

  g_return_if_fail (GB_IS_COMMAND_VIM_PROVIDER (provider));
  g_return_if_fail (completions);
  g_return_if_fail (initial_command_text);

  source_view = get_source_view (provider);

  results = gb_vim_complete (GTK_SOURCE_VIEW (source_view), initial_command_text);
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
