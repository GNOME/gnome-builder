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

/**
 * SECTION:ide-source-snippets-manager
 * @title: IdeSourceSnippetsManager
 * @short_description: Manage snippets for the source code editor
 *
 * The #IdeSourceSnippetsManager is responsible for locating and parsing
 * snippets that are bundled with Builder and user defined snippets.
 *
 * The snippets manager will search various paths and resources for
 * snippets when loading. Snippets are collected per-language so that
 * the editor will only see relevant snippets for the given language.
 *
 * The snippet language is similar to other snippet engines, but with
 * some additional features to make it easier to write snippets for
 * multiple languages at once.
 *
 * Files containing snippets should have a filename suffix of ".snippets".
 *
 * The following makes a snippet called "class" for Python2 and Python3
 * which allows you to tab through edit points. The "$0" contains the
 * final position of the snippet.
 *
 * Each line of the snippet should start with a Tab. When expanding the
 * snippet, tabs will be converted to spaces if the users language settings
 * specify that spaces should be used.
 *
 * |[
 * snippet class
 * - scope python, python3
 * - desc Create a Python class
 * 	class ${1:MyClass}(${2:object}):
 * 		$0
 * ]|
 *
 * The default class name would be "MyClass" and inherit from "object".
 * Upon expanding the snippet, "MyClass" will be focused and "object" will
 * focus once the user hits Tab. A second Tab will exhaust the edit points
 * and therefore place the insertion cursor at "$0".
 *
 * You may reference other edit points as which can help in complex scenarios.
 * In the following example, there will be a single edit point, repeated three
 * times.
 *
 * |[
 * snippet test
 * - scope c
 * - desc An example snippet
 * 	${1:test} $1 $1 $1 $0
 * ]|
 *
 * You may also reference other edit points in the default value for an edit
 * point. This allows you to set a value by default, but allow the user to
 * Tab into that position and modify it.
 *
 * |[
 * snippet test
 * - scope c
 * - desc An example snippet
 * 	${1:foo} ${2:`$1`}
 * ]|
 *
 * If you want to add additional data to the edit point, you can use multiple
 * backticks to include additional text.
 *
 * |[
 * snippet test
 * - scope c
 * - desc An example snippet
 * 	${1:foo} ${2:`$1`_with_`$1`}
 * ]|
 *
 * You can post-process the output text for an edit point by specifying a
 * pipe "|" and then a post-processing function.
 *
 * Currently, the following post-processing functions are supported.
 *
 *  - capitalize: make the input into "Captital Text"
 *  - decapitalize: make the input into "decaptital text"
 *  - html: replaces input "<>" into &amp;lt; and &amp;gt;
 *  - functify: converts input into something that looks like a c_function_name
 *  - namespace: guesses a proper code namespace from the input text
 *  - upper: converts to uppercase
 *  - lower: converts to lowercase
 *  - space: converts the input text into whitespace of the same length
 *  - camelize: converts the input text into CamelCase
 *  - stripsuffix: removes a filename suffix, such as ".txt" from the input
 *  - class: guess the class name from the input text
 *  - instance: guess the instance name from the input text
 *
 * You may chain multiple post-processing functions together.
 *
 * |[
 * snippet test
 * 	${1:some-file} ${2:$1|functify|upper}
 * ]|
 *
 * Since: 3.18
 */

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

/**
 * ide_source_snippets_manager_load_async:
 * @self: a #IdeSourceSnippetsManager
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (nullable): A GAsyncReadyCallback or %NULL
 * @user_data: closure data for @callback
 *
 * Asynchronously locates and parses snippet definitions.
 *
 * Call ide_source_snippets_manager_load_finish() to get the result
 * of this asynchronous operation.
 *
 * Since: 3.18
 */
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
  g_task_set_source_tag (task, ide_source_snippets_manager_load_async);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_run_in_thread (task, ide_source_snippets_manager_load_worker);
}

/**
 * ide_source_snippets_manager_load_finish:
 * @self: a #IdeSourceSnippetsManager
 * @result: a #GAsyncResult provided to the async callback
 * @error: a location for a #GError or %NULL
 *
 * Completes an asynchronous call to ide_source_snippets_manager_load_async().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 *
 * Since: 3.18
 */
gboolean
ide_source_snippets_manager_load_finish (IdeSourceSnippetsManager  *self,
                                         GAsyncResult              *result,
                                         GError                   **error)
{
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * ide_source_snippets_manager_get_for_language_id:
 * @self: an #IdeSourceSnippetsManager
 * @language_id: (not nullable): the identifier for the language
 *
 * Gets the snippets for a given source language.
 *
 * Returns: (transfer none) (nullable): An #IdeSourceSnippets or %NULL.
 *
 * Since: 3.18
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
 * @self: An #IdeSourceSnippetsManager
 * @language_id: (not nullable): a #GtkSourceLanguage
 *
 * Gets the snippets for a given source language.
 *
 * Returns: (transfer none) (nullable): An #IdeSourceSnippets or %NULL.
 *
 * Since: 3.18
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
