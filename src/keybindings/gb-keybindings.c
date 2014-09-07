/* gb-keybindings.c
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

#define G_LOG_DOMAIN "keybindings"

#include <glib/gi18n.h>

#include "gb-log.h"
#include "gb-keybindings.h"

struct _GbKeybindingsPrivate
{
  GHashTable *keybindings;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbKeybindings, gb_keybindings, G_TYPE_OBJECT)

GbKeybindings *
gb_keybindings_new (void)
{
  return g_object_new (GB_TYPE_KEYBINDINGS, NULL);
}

static void
gb_keybindings_load (GbKeybindings *keybindings,
                     GKeyFile      *key_file)
{
  GbKeybindingsPrivate *priv;
  gchar **keys;
  gchar **groups;
  gchar *value;
  guint i;
  guint j;

  g_assert (GB_IS_KEYBINDINGS (keybindings));
  g_assert (key_file);

  priv = keybindings->priv;

  groups = g_key_file_get_groups (key_file, NULL);

  for (i = 0; groups[i]; i++)
    {
      keys = g_key_file_get_keys (key_file, groups[i], NULL, NULL);
      if (!keys)
        continue;

      for (j = 0; keys[j]; j++)
        {
          value = g_key_file_get_string (key_file, groups[i], keys[j], NULL);
          if (!value || !*value)
            continue;

          g_hash_table_replace (priv->keybindings,
                                g_strdup_printf ("%s.%s", groups[i], keys[j]),
                                value);
        }

      g_strfreev (keys);
    }

  g_strfreev (groups);
}

gboolean
gb_keybindings_load_bytes (GbKeybindings *keybindings,
                           GBytes        *bytes,
                           GError       **error)
{
  gconstpointer data;
  GKeyFile *key_file;
  gsize len = 0;
  gboolean ret = FALSE;

  ENTRY;

  g_return_if_fail (GB_IS_KEYBINDINGS (keybindings));
  g_return_if_fail (bytes);

  key_file = g_key_file_new ();
  data = g_bytes_get_data (bytes, &len);
  if (!g_key_file_load_from_data (key_file, data, len,
                                  G_KEY_FILE_NONE, error))
    GOTO (cleanup);

  gb_keybindings_load (keybindings, key_file);

  ret = TRUE;

cleanup:
  g_key_file_free (key_file);

  RETURN (ret);
}

gboolean
gb_keybindings_load_path (GbKeybindings *keybindings,
                          const gchar   *path,
                          GError       **error)
{
  GKeyFile *key_file;
  gboolean ret = FALSE;

  ENTRY;

  g_return_val_if_fail (GB_IS_KEYBINDINGS (keybindings), FALSE);
  g_return_val_if_fail (path, FALSE);

  key_file = g_key_file_new ();

  if (!g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, error))
    GOTO (cleanup);

  gb_keybindings_load (keybindings, key_file);

cleanup:
  g_key_file_free (key_file);

  RETURN (ret);
}

void
gb_keybindings_register (GbKeybindings  *keybindings,
                         GtkApplication *application)
{
  GbKeybindingsPrivate *priv;
  GHashTableIter iter;
  const gchar *action_name;
  const gchar *accelerator;
  gchar *accel_list[2] = { NULL };

  g_return_if_fail (GB_IS_KEYBINDINGS (keybindings));
  g_return_if_fail (GTK_IS_APPLICATION (application));

  priv = keybindings->priv;

  g_hash_table_iter_init (&iter, priv->keybindings);

  while (g_hash_table_iter_next (&iter,
                                 (gpointer *) &action_name,
                                 (gpointer *) &accelerator))
    {
      accel_list[0] = (gchar *) accelerator;
      gtk_application_set_accels_for_action (application,
                                             action_name,
                                             (const gchar* const*)accel_list);
    }
}

static void
gb_keybindings_finalize (GObject *object)
{
  GbKeybindingsPrivate *priv;

  priv = GB_KEYBINDINGS (object)->priv;

  g_clear_pointer (&priv->keybindings, g_hash_table_unref);

  G_OBJECT_CLASS (gb_keybindings_parent_class)->finalize (object);
}

static void
gb_keybindings_class_init (GbKeybindingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_keybindings_finalize;
}

static void
gb_keybindings_init (GbKeybindings *keybindings)
{
  keybindings->priv = gb_keybindings_get_instance_private (keybindings);

  keybindings->priv->keybindings = g_hash_table_new_full (g_str_hash,
                                                          g_str_equal,
                                                          g_free,
                                                          g_free);
}
