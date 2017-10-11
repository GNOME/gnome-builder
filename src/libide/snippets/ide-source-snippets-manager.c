/* ide-source-snippets-manager.c
 *
 * Copyright © 2013 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-source-snippets-manager"

#include <glib/gi18n.h>

#include "ide-global.h"
#include "snippets/ide-source-snippets-manager.h"
#include "snippets/ide-source-snippet-parser.h"
#include "snippets/ide-source-snippets.h"
#include "snippets/ide-source-snippet.h"

struct _IdeSourceSnippetsManager
{
  GObject     parent_instance;
  GHashTable *by_language_id;
};

G_DEFINE_TYPE (IdeSourceSnippetsManager, ide_source_snippets_manager, G_TYPE_OBJECT)

#define SNIPPETS_DIRECTORY "/org/gnome/builder/snippets/"

static gboolean
ide_source_snippets_manager_load_file (IdeSourceSnippetsManager  *self,
                                       GFile                     *file,
                                       GError                   **error)
{
  IdeSourceSnippetParser *parser;
  GList *iter;

  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPETS_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  parser = ide_source_snippet_parser_new ();

  if (!ide_source_snippet_parser_load_from_file (parser, file, error))
    {
      g_object_unref (parser);
      return FALSE;
    }

  for (iter = ide_source_snippet_parser_get_snippets (parser); iter; iter = iter->next)
    {
      IdeSourceSnippets *snippets;
      IdeSourceSnippet *snippet;
      const gchar *language;

      snippet  = iter->data;
      language = ide_source_snippet_get_language (snippet);
      snippets = g_hash_table_lookup (self->by_language_id, language);

      if (!snippets)
        {
          snippets = ide_source_snippets_new ();
          g_hash_table_insert (self->by_language_id, g_strdup (language), snippets);
        }

      ide_source_snippets_add (snippets, snippet);
    }

  g_object_unref (parser);

  return TRUE;
}

static void
ide_source_snippets_manager_load_directory (IdeSourceSnippetsManager *manager,
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
      g_warning (_("Failed to open directory: %s"), error->message);
      g_error_free (error);
      return;
    }

  while ((name = g_dir_read_name (dir)))
    {
      if (g_str_has_suffix (name, ".snippets"))
        {
          filename = g_build_filename (path, name, NULL);
          file = g_file_new_for_path (filename);
          if (!ide_source_snippets_manager_load_file (manager, file, &error))
            {
              g_warning (_("Failed to load file: %s: %s"), filename, error->message);
              g_clear_error (&error);
            }
          g_object_unref (file);
          g_free (filename);
        }
    }

  g_dir_close (dir);
}

static void
ide_source_snippets_manager_load_worker (GTask        *task,
                                         gpointer      source_object,
                                         gpointer      task_data,
                                         GCancellable *cancellable)
{
  g_autofree gchar *path = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_SOURCE_SNIPPETS_MANAGER (source_object));

  /* Load internal snippets */
  path = g_build_filename (g_get_user_config_dir (), ide_get_program_name (), "snippets", NULL);
  g_mkdir_with_parents (path, 0700);
  ide_source_snippets_manager_load_directory (source_object, path);

  g_task_return_boolean (task, TRUE);
}

void
ide_source_snippets_manager_load_async (IdeSourceSnippetsManager *self,
                                        GCancellable             *cancellable,
                                        GAsyncReadyCallback       callback,
                                        gpointer                  user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_SOURCE_SNIPPETS_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_run_in_thread (task, ide_source_snippets_manager_load_worker);
}

gboolean
ide_source_snippets_manager_load_finish (IdeSourceSnippetsManager  *self,
                                         GAsyncResult              *result,
                                         GError                   **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

/**
 * ide_source_snippets_manager_get_for_language_id:
 *
 * Gets the snippets for a given source language.
 *
 * Returns: (transfer none) (nullable): An #IdeSourceSnippets or %NULL.
 */
IdeSourceSnippets *
ide_source_snippets_manager_get_for_language_id (IdeSourceSnippetsManager *self,
                                                 const gchar              *language_id)
{
  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPETS_MANAGER (self), NULL);
  g_return_val_if_fail (language_id != NULL, NULL);

  return g_hash_table_lookup (self->by_language_id, language_id);
}

/**
 * ide_source_snippets_manager_get_for_language:
 *
 * Gets the snippets for a given source language.
 *
 * Returns: (transfer none) (nullable): An #IdeSourceSnippets or %NULL.
 */
IdeSourceSnippets *
ide_source_snippets_manager_get_for_language (IdeSourceSnippetsManager *self,
                                              GtkSourceLanguage        *language)
{
  IdeSourceSnippets *snippets;
  const char *language_id;

  g_return_val_if_fail (IDE_IS_SOURCE_SNIPPETS_MANAGER (self), NULL);
  g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);

  language_id = gtk_source_language_get_id (language);
  snippets = g_hash_table_lookup (self->by_language_id, language_id);

  return snippets;
}

static void
ide_source_snippets_manager_constructed (GObject *object)
{
  IdeSourceSnippetsManager *self = (IdeSourceSnippetsManager *)object;
  GError *error = NULL;
  gchar **names;
  guint i;

  g_assert (IDE_IS_SOURCE_SNIPPETS_MANAGER (self));

  names = g_resources_enumerate_children (SNIPPETS_DIRECTORY, G_RESOURCE_LOOKUP_FLAGS_NONE, &error);

  if (!names)
    {
      g_message ("%s", error->message);
      g_clear_error (&error);
      return;
    }

  for (i = 0; names[i]; i++)
    {
      g_autofree gchar *path = NULL;
      g_autoptr(GFile) file = NULL;

      path = g_strdup_printf ("resource://"SNIPPETS_DIRECTORY"%s", names[i]);
      file = g_file_new_for_uri (path);

      if (!ide_source_snippets_manager_load_file (self, file, &error))
        {
          g_message ("%s", error->message);
          g_clear_error (&error);
        }
    }

  g_strfreev (names);
}

static void
ide_source_snippets_manager_finalize (GObject *object)
{
  IdeSourceSnippetsManager *self = (IdeSourceSnippetsManager *)object;

  g_clear_pointer (&self->by_language_id, g_hash_table_unref);

  G_OBJECT_CLASS (ide_source_snippets_manager_parent_class)->finalize (object);
}

static void
ide_source_snippets_manager_class_init (IdeSourceSnippetsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_source_snippets_manager_constructed;
  object_class->finalize = ide_source_snippets_manager_finalize;
}

static void
ide_source_snippets_manager_init (IdeSourceSnippetsManager *self)
{
  self->by_language_id = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}
