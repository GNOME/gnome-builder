/* ide-python-format-provider.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
 * Copyright © 2015 Elad Alfassa <elad@fedoraproject.org>
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

#define G_LOG_DOMAIN "ide-python-format-provider"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "ide-python-format-provider.h"

struct _IdePythonFormatProvider
{
  IdeObject parent_instance;
};

enum {
  TYPE_NONE,
  TYPE_PERCENTAGE_FORMAT,
  TYPE_DATE_TIME_FORMAT
};

typedef struct
{
  const gchar *format;
  const gchar *description;
} FormatItem;

static void completion_provider_iface_init (GtkSourceCompletionProviderIface *);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdePythonFormatProvider,
                                ide_python_format_provider,
                                IDE_TYPE_OBJECT,
                                0,
                                G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                                       completion_provider_iface_init)
                                G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, NULL))

static const FormatItem DateTimeFormats[] = {
  { "%a", "Weekday as locale’s abbreviated name." },
  { "%A", "Weekday as locale’s full name." },
  { "%w", "Weekday as a decimal number, where 0 is Sunday and 6 is Saturday." },
  { "%d", "Day of the month as a zero-padded decimal number." },
  { "%b", "Month as locale’s abbreviated name." },
  { "%B", "Month as locale’s full name." },
  { "%m", "Month as a zero-padded decimal number." },
  { "%y", "Year without century as a zero-padded decimal number." },
  { "%Y", "Year with century as a decimal number." },
  { "%H", "Hour (24-hour clock) as a zero-padded decimal number." },
  { "%I", "Hour (12-hour clock) as a zero-padded decimal number." },
  { "%p", "Locale’s equivalent of either AM or PM." },
  { "%M", "Minute as a zero-padded decimal number." },
  { "%S", "Second as a zero-padded decimal number." },
  { "%f", "Microsecond as a decimal number, zero-padded on the left." },
  { "%z", "UTC offset in the form +HHMM or -HHMM (empty string if the the object is naive)." },
  { "%Z", "Time zone name (empty string if the object is naive)." },
  { "%j", "Day of the year as a zero-padded decimal number." },
  { "%U", "Week number of the year (Sunday as the first day of the week) as a zero padded decimal number. All days in a new year preceding the first Sunday are considered to be in week." },
  { "%W", "Week number of the year (Monday as the first day of the week) as a decimal number. All days in a new year preceding the first Monday are considered to be in week." },
  { "%c", "Locale’s appropriate date and time representation." },
  { "%x", "Locale’s appropriate date representation." },
  { "%X", "Locale’s appropriate time representation." },
  { "%%", "A literal '%' character." },
  { NULL }
};

static void
ide_python_format_provider_class_init (IdePythonFormatProviderClass *klass)
{
}

static void
ide_python_format_provider_class_finalize (IdePythonFormatProviderClass *klass)
{
}

static void
ide_python_format_provider_init (IdePythonFormatProvider *self)
{
}

static int
guess_type (const GtkTextIter *location)
{
  GtkTextIter iter = *location;
  g_autofree gchar *text = NULL;

  /* walk back to opening ( */
  if (!gtk_text_iter_backward_search (&iter, "(", GTK_TEXT_SEARCH_TEXT_ONLY, &iter, NULL, NULL))
    return TYPE_NONE;

  /* swallow ( */
  if (!gtk_text_iter_backward_char (&iter))
    return TYPE_NONE;

  /* try to find the word previous */
  while (g_unichar_isspace (gtk_text_iter_get_char (&iter)))
    {
      if (!gtk_text_iter_backward_char (&iter))
        return TYPE_NONE;
    }

  /* walk backward to space */
  while (!g_unichar_isspace (gtk_text_iter_get_char (&iter)))
    {
      if (!gtk_text_iter_backward_char (&iter))
        break;
    }

  text = gtk_text_iter_get_slice (&iter, location);

  if (strstr (text, "strftime") || strstr (text, "strptime"))
    return TYPE_DATE_TIME_FORMAT;
  else if (strstr (text, "%"))
    /* We could do a little bit better here: make sure the % comes after a string literal */
    return TYPE_PERCENTAGE_FORMAT;
  else
    return TYPE_NONE;
}


static GList *
create_matches_date_time_format (const gchar *text)
{
  GList *list = NULL;
  gsize i;

  text = strstr (text, "%");

  if (text)
    {
      for (i = 0; DateTimeFormats [i].format; i++)
        {
          if (g_str_has_prefix (DateTimeFormats [i].format, text))
            {
              g_autofree gchar *markup = NULL;

              markup = g_strdup_printf ("%s - %s",
                                        DateTimeFormats [i].format,
                                        DateTimeFormats [i].description);
              list = g_list_prepend (list,
                                     g_object_new (GTK_SOURCE_TYPE_COMPLETION_ITEM,
                                                   "markup", markup,
                                                   "text", DateTimeFormats [i].format,
                                                   NULL));
            }
        }
    }

  return g_list_reverse (list);
}

static GList *
create_matches_percentage_format (const gchar *text)
{
  return NULL;
}

static GList *
create_matches (int type,
                const gchar *text)
{
  switch (type)
    {
    case TYPE_PERCENTAGE_FORMAT:
      return create_matches_percentage_format (text);

    case TYPE_DATE_TIME_FORMAT:
      return create_matches_date_time_format (text);

    case TYPE_NONE:
    default:
      return NULL;
    }
}

static void
ide_python_format_provider_populate (GtkSourceCompletionProvider *provider,
                                     GtkSourceCompletionContext  *context)
{
  GtkSourceBuffer *buffer;
  GtkTextIter iter;
  GList *list = NULL;
  int type;

  if (!gtk_source_completion_context_get_iter (context, &iter))
    goto failure;

  buffer = GTK_SOURCE_BUFFER (gtk_text_iter_get_buffer (&iter));
  g_assert (buffer != NULL);

  if (gtk_source_buffer_iter_has_context_class (buffer, &iter, "string"))
    {
      GtkTextIter line_start = iter;
      GtkTextIter begin;
      GtkTextIter end;

      gtk_text_iter_set_line_offset (&line_start, 0);

      if (gtk_text_iter_backward_search (&iter, "%", GTK_TEXT_SEARCH_TEXT_ONLY,
                                         &begin, &end, &line_start))
        {
          g_autofree gchar *text = NULL;

          if (!gtk_source_buffer_iter_has_context_class (buffer, &begin, "string"))
            goto failure;

          type = guess_type (&begin);
          if (type == TYPE_NONE)
            goto failure;

          text = gtk_text_iter_get_slice (&begin, &iter);
          list = create_matches (type, text);
        }
    }

failure:
  gtk_source_completion_context_add_proposals (context, provider, list, TRUE);
  g_list_free_full (list, g_object_unref);
}

static gchar *
ide_python_format_provider_get_name (GtkSourceCompletionProvider *provider)
{
  return g_strdup (_("Format Strings"));
}

static gboolean
ide_python_format_provider_get_start_iter (GtkSourceCompletionProvider *provider,
                                           GtkSourceCompletionContext  *context,
                                           GtkSourceCompletionProposal *proposal,
                                           GtkTextIter                 *iter)
{
  gtk_source_completion_context_get_iter (context, iter);

  while (gtk_text_iter_get_char (iter) != '%')
    if (!gtk_text_iter_backward_char (iter))
      break;

  return gtk_text_iter_get_char (iter) == '%';
}

static void
completion_provider_iface_init (GtkSourceCompletionProviderIface *iface)
{
  iface->populate = ide_python_format_provider_populate;
  iface->get_name = ide_python_format_provider_get_name;
  iface->get_start_iter = ide_python_format_provider_get_start_iter;
}

void
_ide_python_format_provider_register_type (GTypeModule *module)
{
  ide_python_format_provider_register_type (module);
}
