/* ide-file-settings.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
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

typedef struct
{
  gchar                *encoding;
  IdeFile              *file;
  IdeIndentStyle        indent_style : 2;
  guint                 indent_width : 6;
  guint                 insert_trailing_newline : 1;
  guint                 tab_width : 6;
  guint                 trim_trailing_whitespace : 1;
  GtkSourceNewlineType  newline_type : 2;
} IdeFileSettingsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeFileSettings, ide_file_settings, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ENCODING,
  PROP_FILE,
  PROP_INDENT_STYLE,
  PROP_INDENT_WIDTH,
  PROP_INSERT_TRAILING_NEWLINE,
  PROP_NEWLINE_TYPE,
  PROP_TAB_WIDTH,
  PROP_TRIM_TRAILING_WHITESPACE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

const gchar *
ide_file_settings_get_encoding (IdeFileSettings *self)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_FILE_SETTINGS (self), NULL);

  return priv->encoding;
}

void
ide_file_settings_set_encoding (IdeFileSettings *self,
                                const gchar     *encoding)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_if_fail (IDE_IS_FILE_SETTINGS (self));

  if (priv->encoding != encoding)
    {
      g_free (priv->encoding);
      priv->encoding = g_strdup (encoding);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_ENCODING]);
    }
}

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
IdeFile *
ide_file_settings_get_file (IdeFileSettings *self)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_FILE_SETTINGS (self), NULL);

  return priv->file;
}

static void
ide_file_settings_set_file (IdeFileSettings *self,
                            IdeFile         *file)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_if_fail (IDE_IS_FILE_SETTINGS (self));
  g_return_if_fail (IDE_IS_FILE (file));

  if (priv->file != file)
    {
      if (ide_set_weak_pointer (&priv->file, file))
        g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_FILE]);
    }
}

IdeIndentStyle
ide_file_settings_get_indent_style (IdeFileSettings *self)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_FILE_SETTINGS (self), 0);

  return priv->indent_style;
}

void
ide_file_settings_set_indent_style (IdeFileSettings *self,
                                    IdeIndentStyle   indent_style)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_if_fail (IDE_IS_FILE_SETTINGS (self));
  g_return_if_fail (indent_style >= IDE_INDENT_STYLE_SPACES);
  g_return_if_fail (indent_style <= IDE_INDENT_STYLE_TABS);

  if (priv->indent_style != indent_style)
    {
      priv->indent_style = indent_style;
      g_object_notify_by_pspec (G_OBJECT (self),
                                gParamSpecs [PROP_INDENT_STYLE]);
    }
}

guint
ide_file_settings_get_indent_width (IdeFileSettings *self)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_FILE_SETTINGS (self), 0);

  return priv->indent_width;
}

void
ide_file_settings_set_indent_width (IdeFileSettings *self,
                                    guint            indent_width)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_if_fail (IDE_IS_FILE_SETTINGS (self));
  g_return_if_fail (indent_width > 0);
  g_return_if_fail (indent_width < 32);

  if (priv->indent_width != indent_width)
    {
      priv->indent_width = indent_width;
      g_object_notify_by_pspec (G_OBJECT (self),
                                gParamSpecs [PROP_INDENT_WIDTH]);
    }
}

gboolean
ide_file_settings_get_insert_trailing_newline (IdeFileSettings *self)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_FILE_SETTINGS (self), FALSE);

  return priv->insert_trailing_newline;
}

void
ide_file_settings_set_insert_trailing_newline (IdeFileSettings *self,
                                               gboolean         insert_trailing_newline)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_if_fail (IDE_IS_FILE_SETTINGS (self));

  insert_trailing_newline = !!insert_trailing_newline;

  if (priv->insert_trailing_newline != insert_trailing_newline)
    {
      priv->insert_trailing_newline = insert_trailing_newline;
      g_object_notify_by_pspec (G_OBJECT (self),
                                gParamSpecs [PROP_INSERT_TRAILING_NEWLINE]);
    }
}

GtkSourceNewlineType
ide_file_settings_get_newline_type (IdeFileSettings *self)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_FILE_SETTINGS (self), 0);

  return priv->newline_type;
}

void
ide_file_settings_set_newline_type (IdeFileSettings      *self,
                                    GtkSourceNewlineType  newline_type)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_if_fail (IDE_IS_FILE_SETTINGS (self));
  g_return_if_fail (newline_type >= GTK_SOURCE_NEWLINE_TYPE_LF);
  g_return_if_fail (newline_type <= GTK_SOURCE_NEWLINE_TYPE_CR_LF);

  if (priv->newline_type != newline_type)
    {
      priv->newline_type = newline_type;
      g_object_notify_by_pspec (G_OBJECT (self),
                                gParamSpecs [PROP_NEWLINE_TYPE]);
    }
}

guint
ide_file_settings_get_tab_width (IdeFileSettings *self)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_FILE_SETTINGS (self), 0);

  return priv->tab_width;
}

void
ide_file_settings_set_tab_width (IdeFileSettings *self,
                                 guint            tab_width)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_if_fail (IDE_IS_FILE_SETTINGS (self));
  g_return_if_fail (tab_width > 0);
  g_return_if_fail (tab_width < 32);

  if (priv->tab_width != tab_width)
    {
      priv->tab_width = tab_width;
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_TAB_WIDTH]);
    }
}

gboolean
ide_file_settings_get_trim_trailing_whitespace (IdeFileSettings *self)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_FILE_SETTINGS (self), FALSE);

  return priv->trim_trailing_whitespace;
}

void
ide_file_settings_set_trim_trailing_whitespace (IdeFileSettings *self,
                                                gboolean         trim_trailing_whitespace)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_return_if_fail (IDE_IS_FILE_SETTINGS (self));

  trim_trailing_whitespace = !!trim_trailing_whitespace;

  if (priv->trim_trailing_whitespace != trim_trailing_whitespace)
    {
      priv->trim_trailing_whitespace = trim_trailing_whitespace;
      g_object_notify_by_pspec (G_OBJECT (self),
                                gParamSpecs [PROP_TRIM_TRAILING_WHITESPACE]);
    }
}

static void
ide_file_settings_finalize (GObject *object)
{
  IdeFileSettings *self = (IdeFileSettings *)object;
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  g_clear_pointer (&priv->encoding, g_free);
  ide_clear_weak_pointer (&priv->file);

  G_OBJECT_CLASS (ide_file_settings_parent_class)->finalize (object);
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
    case PROP_ENCODING:
      g_value_set_string (value, ide_file_settings_get_encoding (self));
      break;

    case PROP_FILE:
      g_value_set_object (value, ide_file_settings_get_file (self));
      break;

    case PROP_INDENT_STYLE:
      g_value_set_enum (value, ide_file_settings_get_indent_style (self));
      break;

    case PROP_INDENT_WIDTH:
      g_value_set_uint (value, ide_file_settings_get_indent_width (self));
      break;

    case PROP_INSERT_TRAILING_NEWLINE:
      g_value_set_boolean (value, ide_file_settings_get_insert_trailing_newline (self));
      break;

    case PROP_NEWLINE_TYPE:
      g_value_set_enum (value, ide_file_settings_get_newline_type (self));
      break;

    case PROP_TAB_WIDTH:
      g_value_set_uint (value, ide_file_settings_get_tab_width (self));
      break;

    case PROP_TRIM_TRAILING_WHITESPACE:
      g_value_set_boolean (value, ide_file_settings_get_trim_trailing_whitespace (self));
      break;

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
    case PROP_ENCODING:
      ide_file_settings_set_encoding (self, g_value_get_string (value));
      break;

    case PROP_FILE:
      ide_file_settings_set_file (self, g_value_get_object (value));
      break;

    case PROP_INDENT_STYLE:
      ide_file_settings_set_indent_style (self, g_value_get_enum (value));
      break;

    case PROP_INDENT_WIDTH:
      ide_file_settings_set_indent_width (self, g_value_get_uint (value));
      break;

    case PROP_INSERT_TRAILING_NEWLINE:
      ide_file_settings_set_insert_trailing_newline (self, g_value_get_boolean (value));
      break;

    case PROP_NEWLINE_TYPE:
      ide_file_settings_set_newline_type (self, g_value_get_enum (value));
      break;

    case PROP_TAB_WIDTH:
      ide_file_settings_set_tab_width (self, g_value_get_uint (value));
      break;

    case PROP_TRIM_TRAILING_WHITESPACE:
      ide_file_settings_set_trim_trailing_whitespace (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_file_settings_class_init (IdeFileSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_file_settings_finalize;
  object_class->get_property = ide_file_settings_get_property;
  object_class->set_property = ide_file_settings_set_property;

  gParamSpecs [PROP_ENCODING] =
    g_param_spec_string ("encoding",
                         _("Encoding"),
                         _("The file encoding to use."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ENCODING,
                                   gParamSpecs [PROP_ENCODING]);

  gParamSpecs [PROP_FILE] =
    g_param_spec_object ("file",
                         _("File"),
                         _("The IdeFile the settings represent."),
                         IDE_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FILE,
                                   gParamSpecs [PROP_FILE]);

  gParamSpecs [PROP_INDENT_STYLE] =
    g_param_spec_enum ("indent-style",
                       _("Indent Style"),
                       _("The indent style to use."),
                       IDE_TYPE_INDENT_STYLE,
                       IDE_INDENT_STYLE_SPACES,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_INDENT_STYLE,
                                   gParamSpecs [PROP_INDENT_STYLE]);

  gParamSpecs [PROP_INDENT_WIDTH] =
    g_param_spec_uint ("indent-width",
                       _("Indent Width"),
                       _("The width to use when indenting."),
                       1, 32, 8,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_INDENT_WIDTH,
                                   gParamSpecs [PROP_INDENT_WIDTH]);

  gParamSpecs [PROP_INSERT_TRAILING_NEWLINE] =
    g_param_spec_boolean ("insert-trailing-newline",
                          _("Insert Trailing Newline"),
                          _("If a trailing newline should be implicitly added "
                            "when saving the file."),
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_INSERT_TRAILING_NEWLINE,
                                   gParamSpecs [PROP_INSERT_TRAILING_NEWLINE]);

  gParamSpecs [PROP_NEWLINE_TYPE] =
    g_param_spec_enum ("newline-type",
                       _("Newline Type"),
                       _("The type of newlines to use."),
                       GTK_SOURCE_TYPE_NEWLINE_TYPE,
                       GTK_SOURCE_NEWLINE_TYPE_LF,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_NEWLINE_TYPE,
                                   gParamSpecs [PROP_NEWLINE_TYPE]);

  gParamSpecs [PROP_TAB_WIDTH] =
    g_param_spec_uint ("tab-width",
                       _("Tab Width"),
                       _("The width in characters to represent a tab."),
                       1, 32, 8,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TAB_WIDTH,
                                   gParamSpecs [PROP_TAB_WIDTH]);

  gParamSpecs [PROP_TRIM_TRAILING_WHITESPACE] =
    g_param_spec_boolean ("trim-trailing-whitespace",
                          _("Trim Trailing Whitespace"),
                          _("If trailing whitespace should be trimmed."),
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TRIM_TRAILING_WHITESPACE,
                                   gParamSpecs [PROP_TRIM_TRAILING_WHITESPACE]);
}

static void
ide_file_settings_init (IdeFileSettings *self)
{
  IdeFileSettingsPrivate *priv = ide_file_settings_get_instance_private (self);

  priv->indent_style = IDE_INDENT_STYLE_SPACES;
  priv->indent_width = 8;
  priv->insert_trailing_newline = TRUE;
  priv->newline_type = GTK_SOURCE_NEWLINE_TYPE_LF;
  priv->tab_width = 8;
  priv->trim_trailing_whitespace = TRUE;
}
