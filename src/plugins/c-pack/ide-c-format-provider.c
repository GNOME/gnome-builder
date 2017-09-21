/* ide-c-format-provider.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-c-format-provider"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "ide-c-format-provider.h"

struct _IdeCFormatProvider
{
  IdeObject parent_instance;
};

enum {
  TYPE_NONE,
  TYPE_PRINTF,
  TYPE_SCANF,
  TYPE_STRFTIME,
  TYPE_STRPTIME,
  TYPE_G_DATE_TIME_FORMAT,
};

typedef struct
{
  const gchar *format;
  const gchar *description;
} FormatItem;

static void completion_provider_iface_init (GtkSourceCompletionProviderIface *);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeCFormatProvider,
                                ide_c_format_provider,
                                IDE_TYPE_OBJECT,
                                0,
                                G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                                       completion_provider_iface_init)
                                G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, NULL))

static const FormatItem date_time_formats[] = {
  { "%a", "the abbreviated weekday name according to the current locale" },
  { "%A", "the full weekday name according to the current locale" },
  { "%b", "the abbreviated month name according to the current locale" },
  { "%B", "the full month name according to the current locale" },
  { "%c", "the preferred date and time rpresentation for the current locale" },
  { "%C", "the century number (year/100) as a 2-digit integer (00-99)" },
  { "%d", "the day of the month as a decimal number (range 01 to 31)" },
  { "%e", "the day of the month as a decimal number (range 1 to 31)" },
  { "%F", "equivalent to %Y-%m-%d (the ISO 8601 date format)" },
  { "%g", "the last two digits of the ISO 8601 week-based year as a decimal number (00-99). This works well with %V and %u." },
  { "%G", "the ISO 8601 week-based year as a decimal number. This works well with %V and %u." },
  { "%h", "equivalent to %b" },
  { "%H", "the hour as a decimal number using a 24-hour clock (range 00 to 23)" },
  { "%I", "the hour as a decimal number using a 12-hour clock (range 01 to 12)" },
  { "%j", "the day of the year as a decimal number (range 001 to 366)" },
  { "%k", "the hour (24-hour clock) as a decimal number (range 0 to 23); single digits are preceded by a blank" },
  { "%l", "the hour (12-hour clock) as a decimal number (range 1 to 12); single digits are preceded by a blank" },
  { "%m", "the month as a decimal number (range 01 to 12)" },
  { "%M", "the minute as a decimal number (range 00 to 59)" },
  { "%p", "either \"AM\" or \"PM\" according to the given time value, or the corresponding strings for the current locale. Noon is treated as \"PM\" and midnight as \"AM\"." },
  { "%P", "like %p but lowercase, \"am\" or \"pm\" or a corresponding string for the current locale" },
  { "%r", "the time in a.m. or p.m. notation" },
  { "%R", "the time in 24-hour notation (%H:%M)" },
  { "%s", "the number of seconds since the Epoch, that is, since 1970-01-01 00:00:00 UTC" },
  { "%S", "the second as a decimal number (range 00 to 60)" },
  { "%t", "a tab character" },
  { "%T", "the time in 24-hour notation with seconds (%H:%M:%S)" },
  { "%u", "the ISO 8601 standard day of the week as a decimal, range 1 to 7, Monday being 1. This works well with %G and %V." },
  { "%V", "the ISO 8601 standard week number of the current year as a decimal number, range 01 to 53, where week 1 is the first week that has at least 4 days in the new year. See g_date_time_get_week_of_year(). This works well with %G and %u." },
  { "%w", "the day of the week as a decimal, range 0 to 6, Sunday being 0. This is not the ISO 8601 standard format -- use %u instead." },
  { "%x", "the preferred date representation for the current locale without the time" },
  { "%X", "the preferred time representation for the current locale without the date" },
  { "%y", "the year as a decimal number without the century" },
  { "%Y", "the year as a decimal number including the century" },
  { "%z", "the time zone as an offset from UTC (+hhmm)" },
  { "%:z", "the time zone as an offset from UTC (+hh:mm). This is a gnulib strftime() extension. Since: 2.38" },
  { "%::z", ": the time zone as an offset from UTC (+hh:mm:ss). This is a gnulib strftime() extension. Since: 2.38" },
  { "%:::z", ": the time zone as an offset from UTC, with : to necessary precision (e.g., -04, +05:30). This is a gnulib strftime() extension. Since: 2.38" },
  { "%Z", ": the time zone or name or abbreviation" },
  { "%%", ": a literal % character" },
  { NULL }
};

static void ide_c_format_provider_class_init (IdeCFormatProviderClass *klass) { }
static void ide_c_format_provider_class_finalize (IdeCFormatProviderClass *klass) { }
static void ide_c_format_provider_init (IdeCFormatProvider *self) { }

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

  if (strstr (text, "printf") || strstr (text, "g_print"))
    return TYPE_PRINTF;
  else if (strstr (text, "scanf"))
    return TYPE_SCANF;
  else if (strstr (text, "g_date_time_format"))
    return TYPE_G_DATE_TIME_FORMAT;
  else if (strstr (text, "strftime"))
    return TYPE_STRFTIME;
  else if (strstr (text, "strptime"))
    return TYPE_STRFTIME;
  else
    return TYPE_NONE;
}

static GList *
create_matches_strftime (const gchar *text)
{
  return NULL;
}

static GList *
create_matches_strptime (const gchar *text)
{
  return NULL;
}

static GList *
create_matches_g_date_time_format (const gchar *text)
{
  GList *list = NULL;
  gsize i;

  g_print (">>>> %s\n", text);

  text = strstr (text, "%");

  if (text)
    {
      for (i = 0; date_time_formats [i].format; i++)
        {
          if (g_str_has_prefix (date_time_formats [i].format, text))
            {
              g_autofree gchar *markup = NULL;
              const gchar *insert;

              insert = date_time_formats [i].format + strlen (text);

              markup = g_strdup_printf ("%s - %s",
                                        date_time_formats [i].format,
                                        date_time_formats [i].description);
              list = g_list_prepend (list,
                                     g_object_new (GTK_SOURCE_TYPE_COMPLETION_ITEM,
                                                   "markup", markup,
                                                   "text", insert,
                                                   NULL));
            }
        }
    }

  return g_list_reverse (list);
}

static GList *
create_matches_printf (const gchar *text)
{
  return NULL;
}

static GList *
create_matches_scanf (const gchar *text)
{
  return NULL;
}

static GList *
create_matches (int type,
                const gchar *text)
{
  switch (type)
    {
    case TYPE_STRFTIME:
      return create_matches_strftime (text);

    case TYPE_STRPTIME:
      return create_matches_strptime (text);

    case TYPE_G_DATE_TIME_FORMAT:
      return create_matches_g_date_time_format (text);

    case TYPE_PRINTF:
      return create_matches_printf (text);

    case TYPE_SCANF:
      return create_matches_scanf (text);

    case TYPE_NONE:
    default:
      return NULL;
    }
}

static void
ide_c_format_provider_populate (GtkSourceCompletionProvider *provider,
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
ide_c_format_provider_get_name (GtkSourceCompletionProvider *provider)
{
  return g_strdup (_("Format Strings"));
}

static void
completion_provider_iface_init (GtkSourceCompletionProviderIface *iface)
{
  iface->populate = ide_c_format_provider_populate;
  iface->get_name = ide_c_format_provider_get_name;
}

void
_ide_c_format_provider_register_type (GTypeModule *module)
{
  ide_c_format_provider_register_type (module);
}
