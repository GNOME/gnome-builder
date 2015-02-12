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
#include "ide-language.h"

typedef struct
{
  GFile       *file;
  IdeLanguage *language;
} IdeFilePrivate;

enum
{
  PROP_0,
  PROP_FILE,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeFile, ide_file, IDE_TYPE_OBJECT)

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
  IdeFilePrivate *priv;

  g_return_val_if_fail (IDE_IS_FILE (self), 0);

  priv = ide_file_get_instance_private (self);

  return g_file_hash (priv->file);
}

gboolean
ide_file_equal (IdeFile *self,
                IdeFile *other)
{
  IdeFilePrivate *priv1 = ide_file_get_instance_private (self);
  IdeFilePrivate *priv2 = ide_file_get_instance_private (other);

  g_return_val_if_fail (IDE_IS_FILE (self), FALSE);
  g_return_val_if_fail (IDE_IS_FILE (other), FALSE);

  return g_file_equal (priv1->file, priv2->file);
}

static void
ide_file_create_language (IdeFile *self)
{
  IdeFilePrivate *priv = ide_file_get_instance_private (self);

  g_assert (IDE_IS_FILE (self));

  if (g_once_init_enter (&priv->language))
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
      filename = g_file_get_basename (priv->file);
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

IdeLanguage *
ide_file_get_language (IdeFile *self)
{
  IdeFilePrivate *priv = ide_file_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_FILE (self), NULL);

  if (!priv->language)
    ide_file_create_language (self);

  return priv->language;
}

GFile *
ide_file_get_file (IdeFile *self)
{
  IdeFilePrivate *priv = ide_file_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_FILE (self), NULL);

  return priv->file;
}

static void
ide_file_set_file (IdeFile *self,
                   GFile   *file)
{
  IdeFilePrivate *priv = ide_file_get_instance_private (self);

  g_return_if_fail (IDE_IS_FILE (self));
  g_return_if_fail (G_IS_FILE (file));

  if (file != priv->file)
    {
      if (g_set_object (&priv->file, file))
        g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_FILE]);
    }
}

static void
ide_file_finalize (GObject *object)
{
  IdeFile *self = (IdeFile *)object;
  IdeFilePrivate *priv = ide_file_get_instance_private (self);

  g_clear_object (&priv->file);

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
}

static void
ide_file_init (IdeFile *file)
{
}
