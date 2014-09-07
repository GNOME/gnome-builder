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

#include <glib/gi18n.h>

#include "gb-source-formatter.h"

struct _GbSourceFormatterPrivate
{
  GtkSourceLanguage *language;
};

enum {
  PROP_0,
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

gboolean
gb_source_formatter_format (GbSourceFormatter *formatter,
                            const gchar       *input,
                            gboolean           is_fragment,
                            GCancellable      *cancellable,
                            gchar            **output,
                            GError           **error)
{
  GSubprocessFlags flags;
  GSubprocess *proc;
  gboolean ret = FALSE;
  char *stderr_buf = NULL;

  g_return_val_if_fail (GB_IS_SOURCE_FORMATTER (formatter), FALSE);
  g_return_val_if_fail (input, FALSE);

  /*
   * TODO: Only format C-like code for now.
   */

  flags = (G_SUBPROCESS_FLAGS_STDIN_PIPE |
           G_SUBPROCESS_FLAGS_STDOUT_PIPE |
           G_SUBPROCESS_FLAGS_STDERR_SILENCE);

  proc = g_subprocess_new (flags,
                           error,
                           "uncrustify",
                           "-c",
                           "clutter-uncrustify.cfg",
                           is_fragment ? "--frag" : NULL,
                           NULL);

  if (!g_subprocess_communicate_utf8 (proc, input, cancellable, output, &stderr_buf, error))
    goto finish;

  if (g_subprocess_get_exit_status (proc) != 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   _ ("unrustify failure: %s"),
                   stderr_buf);
      goto finish;
    }

  ret = TRUE;

finish:
  g_clear_object (&proc);
  g_free (stderr_buf);

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
    case PROP_LANGUAGE:
      g_value_set_object (value, gb_source_formatter_get_language (formatter));
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

  gParamSpecs[PROP_LANGUAGE] =
    g_param_spec_object ("language",
                         _ ("Language"),
                         _ ("The language to format."),
                         GTK_SOURCE_TYPE_LANGUAGE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_LANGUAGE,
                                   gParamSpecs[PROP_LANGUAGE]);
}

static void
gb_source_formatter_init (GbSourceFormatter *formatter)
{
  formatter->priv = gb_source_formatter_get_instance_private (formatter);
}
