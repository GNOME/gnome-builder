/*
 * This file is part of gnome-c-utils.
 *
 * Copyright © 2016, 2017 Sébastien Wilmet <swilmet@gnome.org>
 *
 * gnome-c-utils is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gnome-c-utils is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gnome-c-utils.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Basic check of GObject virtual function chain-ups.
 *
 * Usage: gcu-check-chain-ups <file.c>
 *
 * For a less verbose output, redirect stdout to /dev/null. The warnings/errors
 * are printed on stderr.
 *
 * The script searches where a vfunc is chained up, by looking at the following
 * pattern, allowing spaces around the parenthesis and after '->':
 *
 *     _parent_class)->vfunc_name
 *
 * It extracts "vfunc_name". Then the script searches the function name
 * containing the chain-up, and checks that the function name has "vfunc_name"
 * for suffix.
 *
 * For example in this code:
 *
 * static void
 * my_class_finalize (GObject *object)
 * {
 *   ...
 *
 *   G_OBJECT_CLASS (gtk_source_file_loader_parent_class)->dispose (object);
 * }
 *
 * "my_class_finalize" doesn't have the "dispose" suffix, so it'll print a
 * message on stderr.
 */

/* TODO A possible improvement is to search the function name
 * ("my_class_finalize" in the above example) that is present in the pattern:
 *
 * ->foo = function_name;
 *
 * And check that "foo" is the same as vfunc_name, the chained-up vfunc.
 *
 * Of course using a real static analysis tool for the C language would be
 * better.
 */

#include "config.h"

#include <gtksourceview/gtksource.h>
#include <stdlib.h>

#include "gbp-gdiagnose-chainups.h"

static gchar *
get_function_name (const GtkTextIter *_iter)
{
  GtkTextIter iter = *_iter;

  while (gtk_text_iter_backward_line (&iter))
    {
      gunichar c;

      c = gtk_text_iter_get_char (&iter);
      if (g_unichar_isalpha (c) || c == '_')
        {
          GtkTextIter function_name_start;
          GtkTextIter function_name_end;

          function_name_start = iter;
          function_name_end = iter;

          while (gtk_text_iter_forward_char (&function_name_end))
            {
              gunichar end_c;

              end_c = gtk_text_iter_get_char (&function_name_end);
              if (!g_unichar_isalnum (end_c) && end_c != '_')
                break;
            }

          /* A goto label, not a function name. */
          if (gtk_text_iter_get_char (&function_name_end) == ':')
            continue;

          return gtk_text_iter_get_text (&function_name_start, &function_name_end);
        }
    }

  return NULL;
}

static void
check_chain_up (GtkSourceBuffer   *buffer,
                const GtkTextIter *vfunc_start,
                GFile             *file,
                const gchar       *basename,
                IdeDiagnostics    *diagnostics)
{
  gchar *function_name;
  GtkTextIter vfunc_end;
  gchar *vfunc;

  function_name = get_function_name (vfunc_start);
  if (function_name == NULL)
    return;

  if (!gtk_text_iter_starts_word (vfunc_start))
    return;

  vfunc_end = *vfunc_start;
  while (TRUE)
    {
      if (gtk_text_iter_get_char (&vfunc_end) == '_')
        gtk_text_iter_forward_char (&vfunc_end);
      else if (gtk_text_iter_starts_word (&vfunc_end))
        gtk_text_iter_forward_word_end (&vfunc_end);
      else
        break;
    }

  vfunc = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer),
                                    vfunc_start,
                                    &vfunc_end,
                                    FALSE);

  if (!g_str_has_suffix (function_name, vfunc))
    {
      guint begin_line = gtk_text_iter_get_line (vfunc_start);
      guint begin_line_offset = gtk_text_iter_get_line_offset (vfunc_start);
      g_autoptr(IdeDiagnostic) diagnostic = NULL;
      g_autoptr(IdeLocation) location = NULL;
      g_autofree gchar *message = NULL;

      message = g_strdup_printf ("%s() chains up to %s which may be incorrect.",
                                 function_name, vfunc);
      location = ide_location_new (file, begin_line, begin_line_offset);
      diagnostic = ide_diagnostic_new (IDE_DIAGNOSTIC_WARNING, message, location);

      ide_diagnostics_add (diagnostics, diagnostic);
    }

  g_free (function_name);
  g_free (vfunc);
}

void
gbp_gdiagnose_check_chainups (GtkSourceBuffer *buffer,
                              GFile           *file,
                              const char      *basename,
                              IdeDiagnostics  *diagnostics)
{
  GtkSourceSearchSettings *search_settings;
  GtkSourceSearchContext *search_context;
  GtkTextIter iter;
  GtkTextIter match_end;

  g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
  g_return_if_fail (basename != NULL);
  g_return_if_fail (diagnostics != NULL);

  search_settings = gtk_source_search_settings_new ();
  gtk_source_search_settings_set_regex_enabled (search_settings, TRUE);

  /* Search "_parent_class)->", with possible spaces. */
  gtk_source_search_settings_set_search_text (search_settings, "_parent_class\\s*\\)\\s*->\\s*");

  search_context = gtk_source_search_context_new (buffer, search_settings);

  gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

  while (gtk_source_search_context_forward (search_context,
                                            &iter,
                                            NULL,
                                            &match_end,
                                            NULL))
    {
      check_chain_up (buffer, &match_end, file, basename, diagnostics);
      iter = match_end;
    }

  g_object_unref (search_settings);
  g_object_unref (search_context);
}
