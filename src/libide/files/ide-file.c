/* ide-file.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-file"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "files/ide-file.h"
#include "files/ide-file-settings.h"
#include "vcs/ide-vcs.h"

struct _IdeFile
{
  IdeObject          parent_instance;

  gchar             *content_type;
  GFile             *file;
  IdeFileSettings   *file_settings;
  GtkSourceLanguage *language;
  gchar             *path;
  GtkSourceFile     *source_file;
  guint              temporary_id;
};

enum {
  PROP_0,
  PROP_FILE,
  PROP_IS_TEMPORARY,
  PROP_LANGUAGE,
  PROP_PATH,
  PROP_TEMPORARY_ID,
  LAST_PROP
};

DZL_DEFINE_COUNTER (instances, "IdeFile", "Instances", "Number of IdeFile instances.")

G_DEFINE_TYPE (IdeFile, ide_file, IDE_TYPE_OBJECT)

static GParamSpec *properties [LAST_PROP];

G_LOCK_DEFINE (files_cache);
static GHashTable *files_cache;

const gchar *
_ide_file_get_content_type (IdeFile *self)
{
  g_return_val_if_fail (IDE_IS_FILE (self), NULL);

  if (self->content_type != NULL)
    return self->content_type;

  return "text/plain";
}

void
_ide_file_set_content_type (IdeFile     *self,
                            const gchar *content_type)
{
  g_assert (IDE_IS_FILE (self));
  g_assert (content_type);

  if (0 != g_strcmp0 (self->content_type, content_type))
    {
      g_clear_pointer (&self->content_type, g_free);
      g_clear_object (&self->language);
      self->content_type = g_strdup (content_type);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LANGUAGE]);
    }
}

guint
ide_file_hash (IdeFile *self)
{
  g_return_val_if_fail (IDE_IS_FILE (self), 0);

  return g_file_hash (self->file);
}

gboolean
ide_file_equal (IdeFile *self,
                IdeFile *other)
{
  g_return_val_if_fail (IDE_IS_FILE (self), FALSE);
  g_return_val_if_fail (IDE_IS_FILE (other), FALSE);

  return g_file_equal (self->file, other->file);
}

static GtkSourceLanguage *
ide_file_create_language (IdeFile *self)
{
  GtkSourceLanguageManager *manager;
  GtkSourceLanguage *srclang;
  g_autofree gchar *content_type = NULL;
  g_autofree gchar *filename = NULL;
  gboolean uncertain = FALSE;

  g_assert (IDE_IS_FILE (self));

  filename = g_file_get_basename (self->file);

  if (self->content_type)
    content_type = g_strdup (self->content_type);
  else
    content_type = g_content_type_guess (filename, NULL, 0, &uncertain);

  if (uncertain)
    g_clear_pointer (&content_type, g_free);
  else if (self->content_type == NULL)
    self->content_type = g_strdup (content_type);

  manager = gtk_source_language_manager_get_default ();
  srclang = gtk_source_language_manager_guess_language (manager, filename, content_type);

  return srclang;
}

/**
 * ide_file_get_language:
 *
 * Retrieves the #GtkSourceLanguage that was discovered for the file.
 *
 * Returns: (nullable) (transfer none): a #GtkSourceLanguage or %NULL.
 */
GtkSourceLanguage *
ide_file_get_language (IdeFile *self)
{
  g_return_val_if_fail (IDE_IS_FILE (self), NULL);

  if (self->language == NULL)
    {
      GtkSourceLanguage *language;

      language = ide_file_create_language (self);
      self->language = language ? g_object_ref (language) : NULL;
    }

  return self->language;
}

/**
 * ide_file_get_file:
 *
 * Retrieves the underlying #GFile represented by @self.
 *
 * Returns: (transfer none): a #GFile.
 */
GFile *
ide_file_get_file (IdeFile *self)
{
  g_return_val_if_fail (IDE_IS_FILE (self), NULL);

  return self->file;
}

static void
ide_file_set_file (IdeFile *self,
                   GFile   *file)
{
  g_return_if_fail (IDE_IS_FILE (self));
  g_return_if_fail (G_IS_FILE (file));

  if (file != self->file)
    {
      if (g_set_object (&self->file, file))
        g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE]);
    }
}

/**
 * _ide_file_get_source_file:
 * @self: (in): an #IdeFile.
 *
 * Gets the GtkSourceFile for the #IdeFile.
 *
 * Returns: (transfer none): a #GtkSourceFile.
 */
GtkSourceFile *
_ide_file_get_source_file (IdeFile *self)
{
  g_return_val_if_fail (IDE_IS_FILE (self), NULL);

  if (g_once_init_enter (&self->source_file))
    {
      GtkSourceFile *source_file;

      source_file = gtk_source_file_new ();
      gtk_source_file_set_location (source_file, self->file);

      g_once_init_leave (&self->source_file, source_file);
    }

  return self->source_file;
}

const gchar *
ide_file_get_path (IdeFile *self)
{
  g_return_val_if_fail (IDE_IS_FILE (self), NULL);

  if (g_once_init_enter (&self->path))
    {
      IdeContext *context;
      gchar *path = NULL;

      context = ide_object_get_context (IDE_OBJECT (self));

      if (context != NULL)
        {
          IdeVcs *vcs = ide_context_get_vcs (context);
          GFile *workdir = ide_vcs_get_working_directory (vcs);

          if (g_file_has_prefix (self->file, workdir))
            path = g_file_get_relative_path (workdir, self->file);
        }

      if (path == NULL)
        path = g_file_get_path (self->file);

      g_once_init_leave (&self->path, path);
    }

  return self->path;
}

static void
ide_file_set_path (IdeFile     *self,
                   const gchar *path)
{
  g_return_if_fail (IDE_IS_FILE (self));
  g_return_if_fail (!self->path);

  g_clear_pointer (&self->path, g_free);
  self->path = g_strdup (path);
}

static void
ide_file__file_settings_settled_cb (IdeFileSettings *file_settings,
                                    GParamSpec      *pspec,
                                    gpointer         user_data)
{
  g_autoptr(GTask) task = user_data;
  IdeFile *self;

  IDE_ENTRY;

  g_assert (IDE_IS_FILE_SETTINGS (file_settings));
  g_assert (G_IS_TASK (task));
  self = g_task_get_source_object (task);
  g_assert (IDE_IS_FILE (self));

  g_signal_handlers_disconnect_by_func (file_settings,
                                        G_CALLBACK (ide_file__file_settings_settled_cb),
                                        task);
  g_set_object (&self->file_settings, file_settings);
  g_task_return_pointer (task, g_object_ref (file_settings), g_object_unref);

  IDE_EXIT;
}

void
ide_file_load_settings_async (IdeFile              *self,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(IdeFileSettings) file_settings = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_FILE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  /* Use shared instance if available */
  if (self->file_settings != NULL)
    {
      g_task_return_pointer (task, g_object_ref (self->file_settings), g_object_unref);
      IDE_EXIT;
    }

  /* Create our new settings instance, races are okay */
  file_settings = ide_file_settings_new (self);

  /* If this is settled immediately (not using editorconfig), then we can use this now
   * and cache the result for later
   */
  if (ide_file_settings_get_settled (file_settings))
    {
      self->file_settings = g_steal_pointer (&file_settings);
      g_task_return_pointer (task, g_object_ref (self->file_settings), g_object_unref);
      IDE_EXIT;
    }

  /*
   * We need to wait until the settings have settled. editorconfig may need to
   * background load a bunch of .editorconfig files off of disk/sshfs/etc to
   * determine the settings.
   */
  g_signal_connect (file_settings,
                    "notify::settled",
                    G_CALLBACK (ide_file__file_settings_settled_cb),
                    g_object_ref (task));
  g_task_set_task_data (task, g_steal_pointer (&file_settings), g_object_unref);

  IDE_EXIT;
}

/**
 * ide_file_load_settings_finish:
 *
 *
 *
 * Returns: (transfer full): An #IdeFileSettings or %NULL upon failure and
 *   @error is set.
 */
IdeFileSettings *
ide_file_load_settings_finish (IdeFile              *self,
                               GAsyncResult         *result,
                               GError              **error)
{
  IdeFileSettings *ret;

  IDE_ENTRY;

  g_return_val_if_fail (G_IS_TASK (result), NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  IDE_RETURN (ret);
}

/**
 * ide_file_get_temporary_id:
 * @self: (in): an #IdeFile.
 *
 * Gets the #IdeFile:temporary-id property for the file.
 *
 * Temporary files have unique identifiers associated with them so that we can
 * display names such as "unsaved file 1" and know that it will not collide with
 * another temporary file.
 *
 * Files that are not temporary, will return zero.
 *
 * Returns: A positive integer greater than zero if the file is a temporary file.
 */
guint
ide_file_get_temporary_id (IdeFile *self)
{
  g_return_val_if_fail (IDE_IS_FILE (self), 0);

  return self->temporary_id;
}

static void
ide_file_set_temporary_id (IdeFile *self,
                           guint    temporary_id)
{
  g_return_if_fail (IDE_IS_FILE (self));

  self->temporary_id = temporary_id;
}

gboolean
ide_file_get_is_temporary (IdeFile *self)
{
  g_return_val_if_fail (IDE_IS_FILE (self), FALSE);

  return (self->temporary_id != 0);
}

static void
ide_file_dispose (GObject *object)
{
  IdeFile *self = (IdeFile *)object;

  if (self->file != NULL)
    {
      G_LOCK (files_cache);
      if (files_cache != NULL)
        g_hash_table_remove (files_cache, self->file);
      G_UNLOCK (files_cache);
    }

  G_OBJECT_CLASS (ide_file_parent_class)->dispose (object);
}

static void
ide_file_finalize (GObject *object)
{
  IdeFile *self = (IdeFile *)object;

  IDE_ENTRY;

  g_clear_object (&self->file_settings);
  g_clear_object (&self->file);
  g_clear_object (&self->source_file);
  g_clear_object (&self->language);
  g_clear_pointer (&self->path, g_free);
  g_clear_pointer (&self->content_type, g_free);

  G_OBJECT_CLASS (ide_file_parent_class)->finalize (object);

  DZL_COUNTER_DEC (instances);

  IDE_EXIT;
}

static void
ide_file_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  IdeFile *self = (IdeFile *)object;

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, ide_file_get_file (self));
      break;

    case PROP_IS_TEMPORARY:
      g_value_set_boolean (value, ide_file_get_is_temporary (self));
      break;

    case PROP_LANGUAGE:
      g_value_set_object (value, ide_file_get_language (self));
      break;

    case PROP_PATH:
      g_value_set_string (value, ide_file_get_path (self));
      break;

    case PROP_TEMPORARY_ID:
      g_value_set_uint (value, ide_file_get_temporary_id (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_file_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  IdeFile *self = (IdeFile *)object;

  switch (prop_id)
    {
    case PROP_FILE:
      ide_file_set_file (self, g_value_get_object (value));
      break;

    case PROP_PATH:
      ide_file_set_path (self, g_value_get_string (value));
      break;

    case PROP_TEMPORARY_ID:
      ide_file_set_temporary_id (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_file_class_init (IdeFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_file_dispose;
  object_class->finalize = ide_file_finalize;
  object_class->get_property = ide_file_get_property;
  object_class->set_property = ide_file_set_property;

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "The path to the underlying file.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_IS_TEMPORARY] =
    g_param_spec_boolean ("is-temporary",
                          "Is Temporary",
                          "If the file represents a temporary file.",
                         FALSE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_LANGUAGE] =
    g_param_spec_object ("language",
                         "Language",
                         "The file language.",
                         GTK_SOURCE_TYPE_LANGUAGE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PATH] =
    g_param_spec_string ("path",
                         "Path",
                         "The path within the project.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TEMPORARY_ID] =
    g_param_spec_uint ("temporary-id",
                       "Temporary ID",
                       "A unique identifier for temporary files.",
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_file_init (IdeFile *file)
{
  DZL_COUNTER_INC (instances);
}

static gboolean
has_suffix (const gchar          *path,
            const gchar * const *allowed_suffixes)
{
  const gchar *dot;

  dot = strrchr (path, '.');
  if (!dot)
    return FALSE;

  dot++;

  for (guint i = 0; allowed_suffixes [i]; i++)
    {
      if (g_str_equal (dot, allowed_suffixes [i]))
        return TRUE;
    }

  return FALSE;
}

static void
ide_file_find_other_worker (GTask        *task,
                            gpointer      source_object,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
  IdeFile *self = source_object;
  const gchar *src_suffixes[] = { "c", "cc", "cpp", "cxx", NULL };
  const gchar *hdr_suffixes[] = { "h", "hh", "hpp", "hxx", NULL };
  const gchar **target = NULL;
  g_autofree gchar *prefix = NULL;
  g_autofree gchar *uri = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_FILE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  uri = g_file_get_uri (self->file);

  if (has_suffix (uri, src_suffixes))
    {
      target = hdr_suffixes;
    }
  else if (has_suffix (uri, hdr_suffixes))
    {
      target = src_suffixes;
    }
  else
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               "File is missing a suffix.");
      return;
    }

  prefix = g_strndup (uri, strrchr (uri, '.') - uri);

  for (guint i = 0; target [i]; i++)
    {
      g_autofree gchar *new_uri = NULL;
      g_autoptr(GFile) gfile = NULL;

      new_uri = g_strdup_printf ("%s.%s", prefix, target [i]);
      gfile = g_file_new_for_uri (new_uri);

      if (g_file_query_exists (gfile, cancellable))
        {
          IdeContext *context = ide_object_get_context (IDE_OBJECT (self));

          g_task_return_pointer (task,
                                 ide_file_new (context, gfile),
                                 g_object_unref);

          return;
        }
    }

  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_FOUND,
                           "Failed to locate other file.");
}

void
ide_file_find_other_async (IdeFile             *self,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_FILE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_run_in_thread (task, ide_file_find_other_worker);
}

/**
 * ide_file_find_other_finish:
 *
 * Completes an asynchronous call to ide_file_find_other_async(). This function
 * will try to find a matching file for languages where this exists. Such cases
 * include C and C++ where a .c or .cpp file may have a .h or .hh header. Additional
 * suffixes are implemented including (.c, .cc, .cpp, .cxx, .h, .hh, .hpp, and .hxx).
 *
 * Returns an #IdeFile if successful, otherwise %NULL and @error is set.
 *
 * Returns: (transfer full) (nullable): An #IdeFIle or %NULL.
 */
IdeFile *
ide_file_find_other_finish (IdeFile       *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_return_val_if_fail (IDE_IS_FILE (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static IdeFile *
lookup_by_gfile_locked (GFile *file)
{
  IdeFile *ret;

  g_assert (G_IS_FILE (file));

  if (files_cache == NULL)
    files_cache = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, NULL, NULL);

  ret = g_hash_table_lookup (files_cache, file);
  if (ret != NULL)
    g_object_ref (ret);

  return ret;
}

/**
 * ide_file_new:
 * @context: (allow-none): An #IdeContext or %NULL.
 * @file: a #GFile.
 *
 * Creates a new file.
 *
 * Returns: (transfer full): An #IdeFile.
 */
IdeFile *
ide_file_new (IdeContext *context,
              GFile      *file)
{
  g_autoptr(IdeFile) ret = NULL;

  g_return_val_if_fail (!context || IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  G_LOCK (files_cache);

  ret = lookup_by_gfile_locked (file);

  if (ret == NULL)
    {
      ret = g_object_new (IDE_TYPE_FILE,
                          "context", context,
                          "file", file,
                          NULL);
      g_hash_table_insert (files_cache, file, ret);
    }

  G_UNLOCK (files_cache);

  return g_steal_pointer (&ret);
}

IdeFile *
ide_file_new_for_path (IdeContext  *context,
                       const gchar *path)
{
  g_autoptr(GFile) file = NULL;

  g_return_val_if_fail (!context || IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  file = g_file_new_for_path (path);

  return ide_file_new (context, file);
}

gint
ide_file_compare (const IdeFile *a,
                  const IdeFile *b)
{
  g_autofree gchar *filea = NULL;
  g_autofree gchar *fileb = NULL;

  g_assert (a != NULL);
  g_assert (b != NULL);

  filea = g_file_get_uri (a->file);
  fileb = g_file_get_uri (b->file);

  return g_strcmp0 (filea, fileb);
}

const gchar *
ide_file_get_language_id (IdeFile *self)
{
  GtkSourceLanguage *language;

  g_return_val_if_fail (IDE_IS_FILE (self), NULL);

  language = ide_file_get_language (self);

  if (language != NULL)
    return gtk_source_language_get_id (language);

  return NULL;
}
