/* gb-command-manager.c
 *
 * Copyright 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "command-manager"

#include <ide.h>
#include <string.h>

#include "gb-command-manager.h"

struct _GbCommandManager
{
  GObject    parent_instance;

  GPtrArray *providers;
};

G_DEFINE_TYPE (GbCommandManager, gb_command_manager, G_TYPE_OBJECT)

GbCommandManager *
gb_command_manager_new (void)
{
  return g_object_new (GB_TYPE_COMMAND_MANAGER, NULL);
}

static gint
provider_compare_func (gconstpointer a,
                       gconstpointer b)
{
  GbCommandProvider **p1 = (GbCommandProvider **)a;
  GbCommandProvider **p2 = (GbCommandProvider **)b;
  gint i1;
  gint i2;

  i1 = gb_command_provider_get_priority (*p1);
  i2 = gb_command_provider_get_priority (*p2);

  return (i1 - i2);
}

static void
on_notify_priority_cb (GbCommandProvider *provider,
                       GParamSpec        *pspec,
                       GbCommandManager  *manager)
{
  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));
  g_return_if_fail (GB_IS_COMMAND_MANAGER (manager));

  g_ptr_array_sort (manager->providers, provider_compare_func);
}

void
gb_command_manager_add_provider (GbCommandManager  *manager,
                                 GbCommandProvider *provider)
{
  g_return_if_fail (GB_IS_COMMAND_MANAGER (manager));
  g_return_if_fail (GB_IS_COMMAND_PROVIDER (provider));

  g_signal_connect_object (provider, "notify::priority",
                           G_CALLBACK (on_notify_priority_cb),
                           manager, 0);

  g_ptr_array_add (manager->providers, g_object_ref (provider));
  g_ptr_array_sort (manager->providers, provider_compare_func);
}

GbCommand *
gb_command_manager_lookup (GbCommandManager *manager,
                           const gchar      *command_text)
{
  GbCommand *ret = NULL;
  guint i;

  g_return_val_if_fail (GB_IS_COMMAND_MANAGER (manager), NULL);
  g_return_val_if_fail (command_text, NULL);

  for (i = 0; i < manager->providers->len; i++)
    {
      GbCommandProvider *provider;

      provider = g_ptr_array_index (manager->providers, i);
      ret = gb_command_provider_lookup (provider, command_text);

      if (ret)
        return ret;
    }

  return NULL;
}

static gint
sort_strings (const gchar * const * a,
              const gchar * const * b)
{
  return strcmp (*a, *b);
}

gchar **
gb_command_manager_complete (GbCommandManager *manager,
                             const gchar      *initial_command_text)
{
  GPtrArray *completions;

  g_return_val_if_fail (GB_IS_COMMAND_MANAGER (manager), NULL);
  g_return_val_if_fail (initial_command_text, NULL);

  completions = g_ptr_array_new ();

  for (guint i = 0; i < manager->providers->len; i++)
    {
      GbCommandProvider *provider;

      provider = g_ptr_array_index (manager->providers, i);
      gb_command_provider_complete (provider, completions, initial_command_text);
    }

  g_ptr_array_sort (completions, (GCompareFunc)sort_strings);

  g_ptr_array_add (completions, NULL);

  return (gchar **)g_ptr_array_free (completions, FALSE);
}


static void
gb_command_manager_finalize (GObject *object)
{
  GbCommandManager *self = GB_COMMAND_MANAGER (object);

  g_clear_pointer (&self->providers, g_ptr_array_unref);

  G_OBJECT_CLASS (gb_command_manager_parent_class)->finalize (object);
}

static void
gb_command_manager_class_init (GbCommandManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_command_manager_finalize;
}

static void
gb_command_manager_init (GbCommandManager *self)
{
  self->providers = g_ptr_array_new_with_free_func (g_object_unref);
}
