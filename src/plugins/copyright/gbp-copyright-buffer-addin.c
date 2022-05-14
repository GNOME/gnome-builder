/* gbp-copyright-buffer-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
 * Copyright 2022 Tristan Partin <tristan@partin.io>
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

#define G_LOG_DOMAIN "gbp-copyright-buffer-addin"

#include "config.h"

#include <string.h>

#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include <libide-code.h>
#include <libide-gui.h>

#include "gbp-copyright-buffer-addin.h"

#define MAX_LINE 100
#define MAX_BYTES_IN_SCAN (64 << 10) /* 64kb */

static GSettings *copyright_settings;
static GRegex *year_regex;

struct _GbpCopyrightBufferAddin
{
  GObject parent_instance;
};

static void
gbp_copyright_buffer_addin_load (IdeBufferAddin *addin,
                                 IdeBuffer      *buffer)
{
}

static void
gbp_copyright_buffer_addin_unload (IdeBufferAddin *addin,
                                   IdeBuffer      *buffer)
{
}

static void
gbp_copyright_buffer_addin_save_file (IdeBufferAddin *addin,
                                      IdeBuffer      *buffer,
                                      GFile          *file)
{
  GbpCopyrightBufferAddin *self = GBP_COPYRIGHT_BUFFER_ADDIN (addin);
  const char *name;
  g_autoptr(GDateTime) now = NULL;
  g_autofree char *year = NULL;
  GtkTextIter iter, limit;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_COPYRIGHT_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_FILE (file));

  if (!g_settings_get_boolean (copyright_settings, "update-on-save"))
    IDE_EXIT;

  name = g_get_real_name ();
  if (g_strcmp0 (name, "Unknown") == 0)
    IDE_EXIT;

  now = g_date_time_new_now_local ();
  year = g_date_time_format (now, "%Y");

  gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);
  gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (buffer), &limit, MAX_LINE, 0);

  /* Protect against situations where a user may have opened a minified file */
  if ((gtk_text_iter_get_offset (&limit) - gtk_text_iter_get_offset (&iter)) > MAX_BYTES_IN_SCAN)
    IDE_EXIT;

  while (gtk_text_iter_compare (&iter, &limit) < 0)
    {
      GtkTextIter match_begin, match_end;
      g_auto(GStrv) tokens = NULL;
      guint tokens_len;
      g_autofree char *text = NULL;

      if (!gtk_text_iter_forward_search (&iter, name, GTK_TEXT_SEARCH_TEXT_ONLY, &match_begin, &match_end, &limit))
        continue;

      gtk_text_iter_set_line_offset (&match_begin, 0);
      if (!gtk_text_iter_ends_line (&match_end))
        gtk_text_iter_forward_to_line_end (&match_end);

      text = gtk_text_iter_get_slice (&match_begin, &match_end);

      tokens = g_regex_split (year_regex, text, 0);
      /* Constant check for strv length < 2 */
      if (tokens[0] == NULL || tokens[1] == NULL)
        continue;

      tokens_len = g_strv_length (tokens);

      for (guint i = 0; i < tokens_len; i++)
        {
          if (strstr (tokens[i], year))
            IDE_EXIT;
        }

      if (tokens_len >= 2)
        {
          g_autofree char *new_text = NULL;
          g_auto(GStrv) new_tokens = g_new0 (char *, tokens_len + 1);
          guint dash_idx = 0;
          gboolean found_dash = FALSE;

          for (guint i = 0; i < tokens_len; i++)
            {
              if (tokens[i][0] == '-' && tokens[i][1] == 0)
                {
                  dash_idx = i;
                  found_dash = TRUE;

                  break;
                }

              new_tokens[i] = tokens[i];
            }

          if (found_dash)
            {
              new_tokens[dash_idx + 1] = year;
            }
          else
            {
              new_tokens[2] = (char *) "-";
              new_tokens[3] = year;
            }

          new_text = g_strjoinv (NULL, new_tokens);

          gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
          gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &match_begin, &match_end);
          gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &match_begin, new_text, -1);
          gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

          break;
        }

      iter = match_end;
    }

  IDE_EXIT;
}

static void
buffer_addin_init (IdeBufferAddinInterface *iface)
{
  iface->load = gbp_copyright_buffer_addin_load;
  iface->unload = gbp_copyright_buffer_addin_unload;
  iface->save_file = gbp_copyright_buffer_addin_save_file;
}

G_DEFINE_TYPE_WITH_CODE (GbpCopyrightBufferAddin, gbp_copyright_buffer_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_init))

static void
gbp_copyright_buffer_addin_class_init (GbpCopyrightBufferAddinClass *klass)
{
  copyright_settings = g_settings_new("org.gnome.builder.plugins.copyright");
  year_regex = g_regex_new("([0-9]{4})", G_REGEX_OPTIMIZE, 0, NULL);
}

static void
gbp_copyright_buffer_addin_init (GbpCopyrightBufferAddin *self)
{
}
