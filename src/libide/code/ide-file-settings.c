/* ide-file-settings.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-file-settings"

#include "config.h"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "ide-code-enums.h"
#include "ide-file-settings.h"

/*
 * WARNING: This file heavily uses XMACROS.
 *
 * XMACROS are not as difficult as you might imagine. It's basically just an
 * inverstion of macros. We have a defs file (in this case
 * ide-file-settings.defs) which defines information we need about properties.
 * Then we define the macro called from that defs file to do something we need,
 * then include the .defs file.
 *
 * We do that over and over again until we have all the aspects of the object
 * defined.
 */

typedef struct
{
  GPtrArray   *children;
  GFile       *file;
  const gchar *language;
  guint        unsettled_count;

#define IDE_FILE_SETTINGS_PROPERTY(_1, name, field_type, _3, _pname, _4, _5, _6) \
  field_type name;
#include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY

#define IDE_FILE_SETTINGS_PROPERTY(_1, name, field_type, _3, _pname, _4, _5, _6) \
  guint name##_set : 1;
#include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY
} IdeFileSettingsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeFileSettings, ide_file_settings, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_FILE,
  PROP_LANGUAGE,
  PROP_SETTLED,
#define IDE_FILE_SETTINGS_PROPERTY(NAME, _1, _2, _3, _pname, _4, _5, _6) \
  PROP_##NAME, \
  PROP_##NAME##_SET,
#include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

#define IDE_FILE_SETTINGS_PROPERTY(_1, name, _2, ret_type, _pname, _3, _4, _5) \
ret_type ide_file_settings_get_##name (IdeFileSettings *self) \
{ \
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self); \
  g_return_val_if_fail (IDE_IS_FILE_SETTINGS (self), (ret_type)0); \
  if (!ide_file_settings_get_##name##_set (self) && priv->children != NULL) \
    { \
      for (guint i = 0; i < priv->children->len; i++) \
        { \
          IdeFileSettings *child = g_ptr_array_index (priv->children, i); \
          if (ide_file_settings_get_##name##_set (child)) \
            return ide_file_settings_get_##name (child); \
        } \
    } \
  return priv->name; \
}
# include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY

#define IDE_FILE_SETTINGS_PROPERTY(_1, name, field_name, ret_type, _pname, _3, _4, _5) \
gboolean ide_file_settings_get_##name##_set (IdeFileSettings *self) \
{ \
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self); \
  g_return_val_if_fail (IDE_IS_FILE_SETTINGS (self), FALSE); \
  return priv->name##_set; \
}
# include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY

#define IDE_FILE_SETTINGS_PROPERTY(NAME, name, _1, ret_type, _pname, _3, assign_stmt, _4) \
void ide_file_settings_set_##name (IdeFileSettings *self, \
                                   ret_type         name) \
{ \
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self); \
  g_return_if_fail (IDE_IS_FILE_SETTINGS (self)); \
  G_STMT_START { assign_stmt } G_STMT_END; \
  priv->name##_set = TRUE; \
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_##NAME]); \
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_##NAME##_SET]); \
}
# include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY

#define IDE_FILE_SETTINGS_PROPERTY(NAME, name, _1, _2, _pname, _3, _4, _5) \
void ide_file_settings_set_##name##_set (IdeFileSettings *self, \
                                         gboolean         name##_set) \
{ \
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self); \
  g_return_if_fail (IDE_IS_FILE_SETTINGS (self)); \
  priv->name##_set = !!name##_set; \
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_##NAME##_SET]); \
}
# include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY

/**
 * ide_file_settings_get_file:
 * @self: An #IdeFileSettings.
 *
 * Retrieves the underlying file that @self refers to.
 *
 * This may be used by #IdeFileSettings implementations to discover additional
 * information about the settings. For example, a modeline parser might load
 * some portion of the file looking for modelines. An editorconfig
 * implementation might look for ".editorconfig" files.
 *
 * Returns: (transfer none): An #IdeFile.
 */
GFile *
ide_file_settings_get_file (IdeFileSettings *self)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_FILE_SETTINGS (self), NULL);

  return priv->file;
}

static void
ide_file_settings_set_file (IdeFileSettings *self,
                            GFile           *file)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_if_fail (IDE_IS_FILE_SETTINGS (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (priv->file == NULL);

  priv->file = g_object_ref (file);
}

/**
 * ide_file_settings_get_language:
 * @self: a #IdeFileSettings
 *
 * If the language for file settings is known up-front, this will indicate
 * the language identifier known to GtkSourceView such as "c" or "sh".
 *
 * Returns: (nullable): a string containing the language id or %NULL
 */
const gchar *
ide_file_settings_get_language (IdeFileSettings *self)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_FILE_SETTINGS (self), NULL);

  return priv->language;
}

static void
ide_file_settings_set_language (IdeFileSettings *self,
                                const gchar     *language)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_if_fail (IDE_IS_FILE_SETTINGS (self));

  priv->language = g_intern_string (language);
}

/**
 * ide_file_settings_get_settled:
 * @self: An #IdeFileSettings.
 *
 * Gets the #IdeFileSettings:settled property.
 *
 * This property is %TRUE when all of the children file settings have completed loading.
 *
 * Some file setting implementations require that various I/O be performed on disk in
 * the background. This property will change to %TRUE when all of the settings have
 * been loaded.
 *
 * Normally, this is not a problem, since the editor will respond to changes and update them
 * accordingly. However, if you are writing a tool that prints the file settings
 * (such as ide-list-file-settings), you probably want to wait until the values have
 * settled.
 *
 * Returns: %TRUE if all the settings have loaded.
 */
gboolean
ide_file_settings_get_settled (IdeFileSettings *self)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_FILE_SETTINGS (self), FALSE);

  return (priv->unsettled_count == 0);
}

static gchar *
ide_file_settings_repr (IdeObject *object)
{
  IdeFileSettings *self = (IdeFileSettings *)object;
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  if (priv->file != NULL)
    {
      g_autofree gchar *uri = NULL;

      if (g_file_is_native (priv->file))
        return g_strdup_printf ("%s path=\"%s\"",
                                G_OBJECT_TYPE_NAME (self),
                                g_file_peek_path (priv->file));

      uri = g_file_get_uri (priv->file);
      return g_strdup_printf ("%s uri=\"%s\"", G_OBJECT_TYPE_NAME (self), uri);
    }

  return IDE_OBJECT_CLASS (ide_file_settings_parent_class)->repr (object);
}

static void
ide_file_settings_destroy (IdeObject *object)
{
  IdeFileSettings *self = (IdeFileSettings *)object;
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_clear_pointer (&priv->children, g_ptr_array_unref);
  g_clear_pointer (&priv->encoding, g_free);
  g_clear_object (&priv->file);

  IDE_OBJECT_CLASS (ide_file_settings_parent_class)->destroy (object);
}

static void
ide_file_settings_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeFileSettings *self = IDE_FILE_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, ide_file_settings_get_file (self));
      break;

    case PROP_LANGUAGE:
      g_value_set_static_string (value, ide_file_settings_get_language (self));
      break;

    case PROP_SETTLED:
      g_value_set_boolean (value, ide_file_settings_get_settled (self));
      break;

#define IDE_FILE_SETTINGS_PROPERTY(NAME, name, _2, _3, _4, _5, _6, value_type) \
    case PROP_##NAME: \
      g_value_set_##value_type (value, ide_file_settings_get_##name (self)); \
      break;
# include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY

#define IDE_FILE_SETTINGS_PROPERTY(NAME, name, _1, _2, _pname, _3, _4, _5) \
    case PROP_##NAME##_SET: \
      g_value_set_boolean (value, ide_file_settings_get_##name##_set (self)); \
      break;
# include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_file_settings_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeFileSettings *self = IDE_FILE_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_FILE:
      ide_file_settings_set_file (self, g_value_get_object (value));
      break;

    case PROP_LANGUAGE:
      ide_file_settings_set_language (self, g_value_get_string (value));
      break;

#define IDE_FILE_SETTINGS_PROPERTY(NAME, name, _2, _3, _4, _5, _6, value_type) \
    case PROP_##NAME: \
      ide_file_settings_set_##name (self, g_value_get_##value_type (value)); \
      break;
# include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY

#define IDE_FILE_SETTINGS_PROPERTY(NAME, name, _1, _2, _pname, _3, _4, _5) \
    case PROP_##NAME##_SET: \
      ide_file_settings_set_##name##_set (self, g_value_get_boolean (value)); \
      break;
# include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_file_settings_class_init (IdeFileSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->get_property = ide_file_settings_get_property;
  object_class->set_property = ide_file_settings_set_property;

  i_object_class->destroy = ide_file_settings_destroy;
  i_object_class->repr = ide_file_settings_repr;

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "The GFile the settings represent",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LANGUAGE] =
    g_param_spec_string ("language",
                         "Langauge",
                         "The language the settings represent",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SETTLED] =
    g_param_spec_boolean ("settled",
                          "Settled",
                          "If the file settings implementations have settled",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

#define IDE_FILE_SETTINGS_PROPERTY(NAME, name, _1, _2, _pname, pspec, _4, _5) \
  properties [PROP_##NAME] = pspec;
# include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY

#define IDE_FILE_SETTINGS_PROPERTY(NAME, name, _1, _2, _pname, pspec, _4, _5) \
  properties [PROP_##NAME##_SET] = \
    g_param_spec_boolean (_pname"-set", \
                          _pname"-set", \
                          "If IdeFileSettings:"_pname" is set.", \
                          FALSE, \
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
# include "ide-file-settings.defs"
#undef IDE_FILE_SETTINGS_PROPERTY

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_file_settings_init (IdeFileSettings *self)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  priv->indent_style = IDE_INDENT_STYLE_SPACES;
  priv->indent_width = -1;
  priv->insert_trailing_newline = TRUE;
  priv->newline_type = GTK_SOURCE_NEWLINE_TYPE_LF;
  priv->right_margin_position = 80;
  priv->tab_width = 8;
  priv->trim_trailing_whitespace = TRUE;
}

static void
ide_file_settings_child_notify (IdeFileSettings *self,
                                GParamSpec      *pspec,
                                IdeFileSettings *child)
{
  g_assert (IDE_IS_FILE_SETTINGS (self));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_FILE_SETTINGS (child));

  if (pspec->owner_type == IDE_TYPE_FILE_SETTINGS)
    g_object_notify_by_pspec (G_OBJECT (self), pspec);
}

static void
_ide_file_settings_append (IdeFileSettings *self,
                           IdeFileSettings *child)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_assert (IDE_IS_FILE_SETTINGS (self));
  g_assert (IDE_IS_FILE_SETTINGS (child));
  g_assert (self != child);

  g_signal_connect_object (child,
                           "notify",
                           G_CALLBACK (ide_file_settings_child_notify),
                           self,
                           G_CONNECT_SWAPPED);

  if (priv->children == NULL)
    priv->children = g_ptr_array_new_with_free_func (g_object_unref);

  g_ptr_array_add (priv->children, g_object_ref (child));
}

static void
ide_file_settings__init_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  g_autoptr(IdeFileSettings) self = user_data;
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);
  GAsyncInitable *initable = (GAsyncInitable *)object;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_FILE_SETTINGS (self));
  g_assert (G_IS_ASYNC_INITABLE (initable));

  if (!g_async_initable_init_finish (initable, result, &error))
    {
      if (!ide_error_ignore (error))
        g_warning ("%s", error->message);
    }

  if (--priv->unsettled_count == 0)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SETTLED]);
}

IdeFileSettings *
ide_file_settings_new (IdeObject   *parent,
                       GFile       *file,
                       const gchar *language)
{
  IdeFileSettingsPrivate *priv;
  GIOExtensionPoint *extension_point;
  IdeFileSettings *ret;
  GList *list;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (IDE_IS_OBJECT (parent), NULL);

  ret = g_object_new (IDE_TYPE_FILE_SETTINGS,
                      "file", file,
                      "language", language,
                      NULL);
  priv = ide_file_settings_get_instance_private (ret);

  ide_object_append (parent, IDE_OBJECT (ret));

  extension_point = g_io_extension_point_lookup (IDE_FILE_SETTINGS_EXTENSION_POINT);
  list = g_io_extension_point_get_extensions (extension_point);

  /*
   * Don't allow our unsettled count to hit zero until we are finished.
   */
  priv->unsettled_count++;

  for (; list; list = list->next)
    {
      GIOExtension *extension = list->data;
      g_autoptr(IdeFileSettings) child = NULL;
      GType gtype;

      gtype = g_io_extension_get_type (extension);

      if (!g_type_is_a (gtype, IDE_TYPE_FILE_SETTINGS))
        {
          g_warning ("%s is not an IdeFileSettings", g_type_name (gtype));
          continue;
        }

      child = g_object_new (gtype,
                            "file", file,
                            "language", language,
                            NULL);
      ide_object_append (IDE_OBJECT (ret), IDE_OBJECT (child));

      if (G_IS_INITABLE (child))
        {
          g_autoptr(GError) error = NULL;

          if (!g_initable_init (G_INITABLE (child), NULL, &error))
            {
              if (!ide_error_ignore (error))
                g_warning ("%s", error->message);
            }
        }
      else if (G_IS_ASYNC_INITABLE (child))
        {
          priv->unsettled_count++;
          g_async_initable_init_async (G_ASYNC_INITABLE (child),
                                       G_PRIORITY_DEFAULT,
                                       NULL,
                                       ide_file_settings__init_cb,
                                       g_object_ref (ret));
        }

      _ide_file_settings_append (ret, child);
    }

  priv->unsettled_count--;

  return ret;
}
