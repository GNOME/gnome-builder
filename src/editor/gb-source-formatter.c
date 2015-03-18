/* gb-source-formatter.c
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

#define G_LOG_DOMAIN "formatter"
#define UNCRUSTIFY_CONFIG_DIRECTORY "/org/gnome/builder/editor/uncrustify/"

#include <glib/gi18n.h>
#include <ide.h>

#include "gb-source-formatter.h"

struct _GbSourceFormatterPrivate
{
  GtkSourceLanguage *language;
};

enum {
  PROP_0,
  PROP_CAN_FORMAT,
  PROP_LANGUAGE,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceFormatter,
                            gb_source_formatter,
                            G_TYPE_OBJECT)

static GParamSpec * gParamSpecs[LAST_PROP];

GbSourceFormatter *
gb_source_formatter_new_from_language (GtkSourceLanguage *language)
{
  return g_object_new (GB_TYPE_SOURCE_FORMATTER,
                       "language", language,
                       NULL);
}

static gchar *
get_config_path (const gchar *lang_id)
{
  gchar *path;
  gchar *name;

  name = g_strdup_printf ("uncrustify.%s.cfg", lang_id);
  path = g_build_filename (g_get_user_config_dir (),
                           "gnome-builder", "uncrustify", name,
                           NULL);
  g_free (name);

  return path;
}

gboolean
gb_source_formatter_get_can_format (GbSourceFormatter *formatter)
{
  GtkSourceLanguage *language;
  const gchar *lang_id;
  gboolean ret;
  gchar *path;

  g_return_val_if_fail (GB_IS_SOURCE_FORMATTER (formatter), FALSE);

  language = formatter->priv->language;

  if (!language)
    return FALSE;

  lang_id = gtk_source_language_get_id (language);
  path = get_config_path (lang_id);

  ret = g_file_test (path, G_FILE_TEST_IS_REGULAR);

  g_free (path);

  return ret;
}

gboolean
gb_source_formatter_format (GbSourceFormatter *formatter,
                            const gchar       *input,
                            gboolean           is_fragment,
                            GCancellable      *cancellable,
                            gchar            **output,
                            GError           **error)
{
  GbSourceFormatterPrivate *priv;
  GSubprocessFlags flags;
  GSubprocess *proc;
  gboolean ret = FALSE;
  gchar *stderr_buf = NULL;
  gchar *config_path;

  g_return_val_if_fail (GB_IS_SOURCE_FORMATTER (formatter), FALSE);
  g_return_val_if_fail (input, FALSE);

  priv = formatter->priv;

  if (!gb_source_formatter_get_can_format (formatter))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   _("Failed to locate uncrustify configuration."));
      return FALSE;
    }

  config_path = get_config_path (gtk_source_language_get_id (priv->language));

  flags = (G_SUBPROCESS_FLAGS_STDIN_PIPE |
           G_SUBPROCESS_FLAGS_STDOUT_PIPE |
           G_SUBPROCESS_FLAGS_STDERR_SILENCE);

  proc = g_subprocess_new (flags,
                           error,
                           "uncrustify",
                           "-c",
                           config_path,
                           is_fragment ? "--frag" : NULL,
                           NULL);

  if (!g_subprocess_communicate_utf8 (proc, input, cancellable, output, &stderr_buf, error))
    goto finish;

  if (g_subprocess_get_exit_status (proc) != 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   _("uncrustify failure: %s"),
                   stderr_buf);
      goto finish;
    }

  ret = TRUE;

finish:
  g_clear_object (&proc);
  g_free (stderr_buf);
  g_free (config_path);

  return ret;
}

GtkSourceLanguage *
gb_source_formatter_get_language (GbSourceFormatter *formatter)
{
  g_return_val_if_fail (GB_IS_SOURCE_FORMATTER (formatter), NULL);

  return formatter->priv->language;
}

void
gb_source_formatter_set_language (GbSourceFormatter *formatter,
                                  GtkSourceLanguage *language)
{
  GbSourceFormatterPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_FORMATTER (formatter));
  g_return_if_fail (!language || GTK_SOURCE_IS_LANGUAGE (language));

  priv = formatter->priv;

  if (language)
    g_object_ref (language);

  g_clear_object (&priv->language);

  priv->language = language;

  g_object_notify_by_pspec (G_OBJECT (formatter), gParamSpecs[PROP_LANGUAGE]);
  g_object_notify_by_pspec (G_OBJECT (formatter), gParamSpecs[PROP_CAN_FORMAT]);
}

static void
gb_source_formatter_extract_configs (void)
{
  GError *error = NULL;
  gchar **names;
  gchar *target_dir;
  guint i;

  IDE_ENTRY;

  target_dir = g_build_filename (g_get_user_config_dir (),
                                 "gnome-builder", "uncrustify",
                                 NULL);

  if (!g_file_test (target_dir, G_FILE_TEST_IS_DIR))
    {
      g_mkdir_with_parents (target_dir, 0750);
    }

  names = g_resources_enumerate_children (UNCRUSTIFY_CONFIG_DIRECTORY,
					  G_RESOURCE_LOOKUP_FLAGS_NONE,
					  &error);

  if (!names)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      IDE_GOTO (cleanup);
    }

  for (i = 0; names [i]; i++)
    {
      GFile *file;
      gchar *target_path;
      gchar *uri;

      uri = g_strdup_printf ("resource://"UNCRUSTIFY_CONFIG_DIRECTORY"%s",
                             names [i]);
      file = g_file_new_for_uri (uri);

      target_path = g_build_filename (g_get_user_config_dir (),
                                      "gnome-builder", "uncrustify", names [i],
                                      NULL);

      if (!g_file_test (target_path, G_FILE_TEST_IS_REGULAR))
        {
          GFile *target_file;

          target_file = g_file_new_for_path (target_path);
          if (!g_file_copy (file, target_file, G_FILE_COPY_NONE, NULL, NULL,
                            NULL, &error))
            {
              g_warning (_("Failure copying to \"%s\": %s"), target_path,
                         error->message);
              g_clear_error (&error);
            }

          g_clear_object (&target_file);
        }

      g_clear_object (&file);
      g_free (uri);
      g_free (target_path);
    }

cleanup:
  g_strfreev (names);
  g_free (target_dir);

  IDE_EXIT;
}

static void
gb_source_formatter_finalize (GObject *object)
{
  GbSourceFormatterPrivate *priv;

  priv = GB_SOURCE_FORMATTER (object)->priv;

  g_clear_object (&priv->language);

  G_OBJECT_CLASS (gb_source_formatter_parent_class)->finalize (object);
}

static void
gb_source_formatter_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbSourceFormatter *formatter = GB_SOURCE_FORMATTER (object);

  switch (prop_id)
    {
    case PROP_CAN_FORMAT:
      g_value_set_boolean (value,
                           gb_source_formatter_get_can_format (formatter));
      break;
    case PROP_LANGUAGE:
      g_value_set_object (value,
                          gb_source_formatter_get_language (formatter));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_formatter_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbSourceFormatter *formatter = GB_SOURCE_FORMATTER (object);

  switch (prop_id)
    {
    case PROP_LANGUAGE:
      gb_source_formatter_set_language (formatter, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_formatter_class_init (GbSourceFormatterClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gb_source_formatter_finalize;
  object_class->get_property = gb_source_formatter_get_property;
  object_class->set_property = gb_source_formatter_set_property;

  gParamSpecs [PROP_CAN_FORMAT] =
    g_param_spec_boolean ("can-format",
                          _("Can Format"),
                          _("If the source language can be formatted."),
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CAN_FORMAT,
                                   gParamSpecs [PROP_CAN_FORMAT]);

  gParamSpecs[PROP_LANGUAGE] =
    g_param_spec_object ("language",
                         _ ("Language"),
                         _ ("The language to format."),
                         GTK_SOURCE_TYPE_LANGUAGE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_LANGUAGE,
                                   gParamSpecs[PROP_LANGUAGE]);

  gb_source_formatter_extract_configs ();
}

static void
gb_source_formatter_init (GbSourceFormatter *formatter)
{
  formatter->priv = gb_source_formatter_get_instance_private (formatter);
}
