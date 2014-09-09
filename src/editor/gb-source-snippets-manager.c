/* gb-source-snippets-manager.c
 *
 * Copyright (C) 2013 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "snippets"

#include <glib/gi18n.h>

#include "gb-source-snippets-manager.h"

struct _GbSourceSnippetsManagerPrivate
{
  GHashTable *by_language_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceSnippetsManager,
                            gb_source_snippets_manager,
                            G_TYPE_OBJECT)

#define SNIPPETS_DIRECTORY "/org/gnome/builder/snippets/"

static gboolean
gb_source_snippets_manager_load_file (GbSourceSnippetsManager *manager,
                                      GFile                   *file,
                                      const gchar             *force_lang,
                                      GError                 **error)
{
  GbSourceSnippets *snippets;
  gchar *base = NULL;

  g_return_val_if_fail (GB_IS_SOURCE_SNIPPETS_MANAGER (manager), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (!force_lang)
    {
      base = g_file_get_basename (file);

      if (!base)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVAL,
                       _("The file is invalid."));
          return FALSE;
        }

      if (strstr (base, "."))
        *strstr (base, ".") = '\0';

      force_lang = base;
    }

  snippets = g_hash_table_lookup (manager->priv->by_language_id, force_lang);

  if (!snippets)
    {
      snippets = gb_source_snippets_new ();
      g_hash_table_insert (manager->priv->by_language_id,
                           g_strdup (force_lang),
                           snippets);
    }

  g_free (base);

  if (!gb_source_snippets_load_from_file (snippets, file, error))
    return FALSE;

  return TRUE;
}

static void
gb_source_snippets_manager_load_directory (GbSourceSnippetsManager *manager,
                                           const gchar             *path)
{
  const gchar *name;
  GError *error = NULL;
  gchar *filename;
  GFile *file;
  GDir *dir;

  dir = g_dir_open (path, 0, &error);
  if (!dir)
    {
      g_warning (_("Failed to open directory: %s"),
                 error->message);
      g_error_free (error);
      return;
    }

  while ((name = g_dir_read_name (dir)))
    {
      if (g_str_has_suffix (name, ".snippets"))
        {
          filename = g_build_filename (path, name, NULL);
          file = g_file_new_for_path (filename);
          if (!gb_source_snippets_manager_load_file (manager, file,
                                                     NULL, &error))
            {
              g_warning (_("Failed to load file: %s: %s"),
                         filename, error->message);
              g_clear_error (&error);
            }
          g_object_unref (file);
          g_free (filename);
        }
    }

  g_dir_close (dir);
}

GbSourceSnippetsManager *
gb_source_snippets_manager_get_default (void)
{
  static GbSourceSnippetsManager *instance;
  gchar *path;

  if (!instance)
    {
      instance = g_object_new (GB_TYPE_SOURCE_SNIPPETS_MANAGER, NULL);
      path = g_build_filename (g_get_user_config_dir (),
                               "gnome-builder",
                               "snippets",
                               NULL);
      g_mkdir_with_parents (path, 0700);
      gb_source_snippets_manager_load_directory (instance, path);
      g_free (path);
      g_object_add_weak_pointer (G_OBJECT (instance),
                                 (gpointer *) &instance);
    }

  return instance;
}

GbSourceSnippets *
gb_source_snippets_manager_get_for_language (GbSourceSnippetsManager *manager,
                                             GtkSourceLanguage       *language)
{
  GbSourceSnippetsManagerPrivate *priv;
  GbSourceSnippets *snippets;
  const char *language_id;

  g_return_val_if_fail (GB_IS_SOURCE_SNIPPETS_MANAGER (manager), NULL);
  g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);

  priv = manager->priv;

  language_id = gtk_source_language_get_id (language);
  snippets = g_hash_table_lookup (priv->by_language_id, language_id);

  if (!snippets && g_str_equal (language_id, "chdr"))
    snippets = g_hash_table_lookup (priv->by_language_id, "c");

  return snippets;
}

static void
gb_source_snippets_manager_preload_c (GbSourceSnippetsManager *manager)
{
  GFile *file;
  gchar *path;

  g_return_if_fail (GB_IS_SOURCE_SNIPPETS_MANAGER (manager));

  file = g_file_new_for_uri ("resource://"SNIPPETS_DIRECTORY"c.snippets");
  if (g_file_query_exists (file, NULL))
    gb_source_snippets_manager_load_file (manager, file, "chdr", NULL);
  g_clear_object (&file);

  path = g_build_filename (g_get_user_config_dir (), "gnome-builder",
                           "snippets", "c.snippets", NULL);
  file = g_file_new_for_path (path);
  if (g_file_query_exists (file, NULL))
    gb_source_snippets_manager_load_file (manager, file, "chdr", NULL);
  g_clear_object (&file);
  g_free (path);
}

static void
gb_source_snippets_manager_constructed (GObject *object)
{
  GbSourceSnippetsManager *manager = (GbSourceSnippetsManager *)object;
  GError *error = NULL;
  GFile *file;
  gchar *path;
  gchar **names;
  guint i;

  g_assert (GB_IS_SOURCE_SNIPPETS_MANAGER (manager));

  /*
   * We need to preload chdr so that it is the combination of the "c"
   * snippets and the chdr snippets on top of that. This way, you don't
   * need to write all of your snippets twice, for both "c" and "chdr".
   */
  gb_source_snippets_manager_preload_c (manager);

  names = g_resources_enumerate_children (SNIPPETS_DIRECTORY,
                                          G_RESOURCE_LOOKUP_FLAGS_NONE,
                                          &error);

  if (!names)
    {
      g_message ("%s", error->message);
      g_clear_error (&error);
      return;
    }

  for (i = 0; names[i]; i++)
    {
      path = g_strdup_printf ("resource://"SNIPPETS_DIRECTORY"%s", names[i]);
      file = g_file_new_for_uri (path);
      if (!gb_source_snippets_manager_load_file (manager, file, NULL, &error))
        {
          g_message ("%s", error->message);
          g_clear_error (&error);
        }
      g_free (path);
      g_clear_object (&file);
    }

  g_strfreev (names);
}

static void
gb_source_snippets_manager_finalize (GObject *object)
{
  GbSourceSnippetsManagerPrivate *priv;

  priv = GB_SOURCE_SNIPPETS_MANAGER (object)->priv;

  g_clear_pointer (&priv->by_language_id, g_hash_table_unref);

  G_OBJECT_CLASS (gb_source_snippets_manager_parent_class)->finalize (object);
}

static void
gb_source_snippets_manager_class_init (GbSourceSnippetsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gb_source_snippets_manager_constructed;
  object_class->finalize = gb_source_snippets_manager_finalize;
}

static void
gb_source_snippets_manager_init (GbSourceSnippetsManager *manager)
{
  manager->priv = gb_source_snippets_manager_get_instance_private (manager);

  manager->priv->by_language_id =
    g_hash_table_new_full (g_str_hash,
                           g_str_equal,
                           g_free,
                           g_object_unref);
}
