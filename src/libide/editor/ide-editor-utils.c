/* ide-editor-utils.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-utils"

#include "config.h"

#include <glib/gi18n.h>
#include <string.h>
#include <math.h>

#include "ide-editor-utils.h"

static const struct {
  GtkSourceNewlineType type;
  const char *id;
  const char *label;
} line_endings[] = {
  { GTK_SOURCE_NEWLINE_TYPE_LF, "unix", N_("Unix/Linux (LF)") },
  { GTK_SOURCE_NEWLINE_TYPE_CR, "mac", N_("Mac OS Classic (CR)") },
  { GTK_SOURCE_NEWLINE_TYPE_CR_LF, "windows", N_("Windows (CR+LF)") },
};

static int
sort_by_name (gconstpointer a,
              gconstpointer b)
{
  return g_strcmp0 (gtk_source_encoding_get_name (a),
                    gtk_source_encoding_get_name (b));
}

void
ide_editor_file_chooser_add_encodings (GtkFileChooser *chooser)
{
  GPtrArray *choices;
  GPtrArray *labels;
  GSList *all;

  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  all = g_slist_sort (gtk_source_encoding_get_all (), sort_by_name);
  choices = g_ptr_array_new ();
  labels = g_ptr_array_new_with_free_func (g_free);

#define ADD_ENCODING(id, name)             \
  G_STMT_START {                           \
    g_ptr_array_add(choices, (char *)id);  \
    g_ptr_array_add(labels, (char *)name); \
  } G_STMT_END

  ADD_ENCODING ("auto", g_strdup (N_("Automatically Detected")));

  for (const GSList *l = all; l; l = l->next)
    {
      GtkSourceEncoding *encoding = l->data;
      char *title = g_strdup_printf ("%s (%s)",
                                     gtk_source_encoding_get_name (encoding),
                                     gtk_source_encoding_get_charset (encoding));
      ADD_ENCODING (gtk_source_encoding_get_charset (encoding), title);
    }

  ADD_ENCODING (NULL, NULL);
#undef ADD_ENCODING

  gtk_file_chooser_add_choice (chooser,
                               "encoding",
                               _("Character Encoding:"),
                               (const char **)(gpointer)choices->pdata,
                               (const char **)(gpointer)labels->pdata);
  gtk_file_chooser_set_choice (chooser, "encoding", "auto");

  g_slist_free (all);
  g_clear_pointer (&choices, g_ptr_array_unref);
  g_clear_pointer (&labels, g_ptr_array_unref);
}

void
ide_editor_file_chooser_add_line_endings (GtkFileChooser       *chooser,
                                          GtkSourceNewlineType  selected)
{
  static GArray *choices;
  static GArray *labels;

  g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

  if (choices == NULL)
    {
      choices = g_array_new (TRUE, FALSE, sizeof (char *));
      labels = g_array_new (TRUE, FALSE, sizeof (char *));

      for (guint i = 0; i < G_N_ELEMENTS (line_endings); i++)
        {
          const char *msg = g_dgettext (GETTEXT_PACKAGE, line_endings[i].label);

          g_array_append_val (choices, line_endings[i].id);
          g_array_append_val (labels, msg);
        }
    }

  gtk_file_chooser_add_choice (chooser,
                               "line-ending",
                               _("Line Ending:"),
                               (const char **)(gpointer)choices->data,
                               (const char **)(gpointer)labels->data);
  gtk_file_chooser_set_choice (chooser, "line-endings", "unix");

  for (guint i = 0; i < G_N_ELEMENTS (line_endings); i++)
    {
      if (line_endings[i].type == selected)
        {
          gtk_file_chooser_set_choice (chooser, "line-endings", line_endings[i].id);
          break;
        }
    }
}

const GtkSourceEncoding *
ide_editor_file_chooser_get_encoding (GtkFileChooser *chooser)
{
  const char *encoding;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), NULL);

  if ((encoding = gtk_file_chooser_get_choice (chooser, "encoding")))
    {
      if (strcmp (encoding, "auto") != 0)
        return gtk_source_encoding_get_from_charset (encoding);
    }

  return NULL;
}

GtkSourceNewlineType
ide_editor_file_chooser_get_line_ending (GtkFileChooser *chooser)
{
  const char *ending;

  g_return_val_if_fail (GTK_IS_FILE_CHOOSER (chooser), 0);

  if ((ending = gtk_file_chooser_get_choice (chooser, "line-ending")))
    {
      for (guint i = 0; i < G_N_ELEMENTS (line_endings); i++)
        {
          if (g_strcmp0 (ending, line_endings[i].id) == 0)
            return line_endings[i].type;
        }
    }

  return GTK_SOURCE_NEWLINE_TYPE_LF;
}
