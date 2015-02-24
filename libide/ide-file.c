/* ide-file.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "ide-file.h"
#include "ide-file-settings.h"
#include "ide-language.h"

struct _IdeFile
{
  IdeObject      parent_instance;

  GFile         *file;
  IdeLanguage   *language;
  gchar         *path;
};

enum
{
  PROP_0,
  PROP_FILE,
  PROP_LANGUAGE,
  PROP_PATH,
  LAST_PROP
};

G_DEFINE_TYPE (IdeFile, ide_file, IDE_TYPE_OBJECT)

static GParamSpec *gParamSpecs [LAST_PROP];

static const gchar *
ide_file_remap_language (const gchar *lang_id)
{
  if (!lang_id)
    return NULL;

  if (g_str_equal (lang_id, "chdr") ||
      g_str_equal (lang_id, "cpp"))
    return "c";

  if (g_str_equal (lang_id, "python3"))
    return "python";

  return lang_id;
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

static void
ide_file_create_language (IdeFile *self)
{
  g_assert (IDE_IS_FILE (self));

  if (g_once_init_enter (&self->language))
    {
      GtkSourceLanguageManager *manager;
      GtkSourceLanguage *srclang;
      IdeLanguage *language = NULL;
      const gchar *lang_id = NULL;
      g_autoptr(gchar) content_type = NULL;
      g_autoptr(gchar) filename = NULL;
      IdeContext *context;
      gboolean uncertain = TRUE;

      context = ide_object_get_context (IDE_OBJECT (self));
      filename = g_file_get_basename (self->file);
      content_type = g_content_type_guess (filename, NULL, 0, &uncertain);

      if (uncertain)
        g_clear_pointer (&content_type, g_free);

      manager = gtk_source_language_manager_get_default ();
      srclang = gtk_source_language_manager_guess_language (manager, filename, content_type);

      if (srclang)
        {
          g_autoptr(gchar) ext_name = NULL;
          GIOExtension *extension;
          GIOExtensionPoint *point;
          const gchar *lookup_id;

          lang_id = gtk_source_language_get_id (srclang);
          lookup_id = ide_file_remap_language (lang_id);
          ext_name = g_strdup_printf (IDE_LANGUAGE_EXTENSION_POINT".%s", lookup_id);
          point = g_io_extension_point_lookup (IDE_LANGUAGE_EXTENSION_POINT);
          extension = g_io_extension_point_get_extension_by_name (point, ext_name);

          if (extension)
            {
              GType type_id;

              type_id = g_io_extension_get_type (extension);

              if (g_type_is_a (type_id, IDE_TYPE_LANGUAGE))
                language = g_initable_new (type_id, NULL, NULL,
                                           "context", context,
                                           "id", lang_id,
                                           NULL);
              else
                g_warning (_("Type \"%s\" is not an IdeLanguage."),
                           g_type_name (type_id));
            }
        }

      if (!language)
        language = g_object_new (IDE_TYPE_LANGUAGE,
                                 "context", context,
                                 "id", lang_id,
                                 NULL);

      g_once_init_leave (&priv->language, language);
    }
}

/**
 * ide_file_get_language:
 *
 * Retrieves the #IdeLanguage that was discovered for the file. In some cases,
 * this will be a subclass of #IdeLanguage, such as #IdeCLanguage.
 *
 * Returns: (transfer none): An #IdeLanguage
 */
IdeLanguage *
ide_file_get_language (IdeFile *self)
{
  g_return_val_if_fail (IDE_IS_FILE (self), NULL);

  if (!self->language)
    ide_file_create_language (self);

  return self->language;
}

/**
 * ide_file_get_file:
 *
 * Retrieves the underlying #GFile represented by @self.
 *
 * Returns: (transfer none): A #GFile.
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
        g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_FILE]);
    }
}

const gchar *
ide_file_get_path (IdeFile *self)
{
  g_return_val_if_fail (IDE_IS_FILE (self), NULL);

  return self->path;
}

static void
ide_file_set_path (IdeFile     *self,
                   const gchar *path)
{
  g_return_if_fail (IDE_IS_FILE (self));
  g_return_if_fail (!self->path);

  self->path = g_strdup (path);
}

static void
ide_file_load_settings_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeObject) ret = NULL;
  GError *error = NULL;

  g_return_if_fail (G_IS_TASK (task));

  ret = ide_object_new_finish (result, &error);

  if (!ret)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, g_object_ref (ret), g_object_unref);
}

void
ide_file_load_settings_async (IdeFile              *self,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data)
{
  IdeContext *context;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_FILE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  context = ide_object_get_context (IDE_OBJECT (self));
  ide_object_new_async (IDE_FILE_SETTINGS_EXTENSION_POINT,
                        G_PRIORITY_DEFAULT,
                        cancellable,
                        ide_file_load_settings_cb,
                        g_object_ref (task),
                        "context", context,
                        "file", self,
                        NULL);
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
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_file_finalize (GObject *object)
{
  IdeFile *self = (IdeFile *)object;

  g_clear_object (&self->file);
  g_clear_object (&self->language);
  g_clear_pointer (&self->path, g_free);

  G_OBJECT_CLASS (ide_file_parent_class)->finalize (object);
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

    case PROP_LANGUAGE:
      g_value_set_object (value, ide_file_get_language (self));
      break;

    case PROP_PATH:
      g_value_set_string (value, ide_file_get_path (self));
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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_file_class_init (IdeFileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_file_finalize;
  object_class->get_property = ide_file_get_property;
  object_class->set_property = ide_file_set_property;

  gParamSpecs [PROP_FILE] =
    g_param_spec_object ("file",
                         _("File"),
                         _("The path to the underlying file."),
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FILE,
                                   gParamSpecs [PROP_FILE]);

  gParamSpecs [PROP_LANGUAGE] =
    g_param_spec_object ("language",
                         _("Language"),
                         _("The file language."),
                         IDE_TYPE_LANGUAGE,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_LANGUAGE,
                                   gParamSpecs [PROP_LANGUAGE]);

  gParamSpecs [PROP_PATH] =
    g_param_spec_string ("path",
                         _("Path"),
                         _("The path within the project."),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PATH,
                                   gParamSpecs [PROP_PATH]);
}

static void
ide_file_init (IdeFile *file)
{
}
