/* gb-vim.c
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

#define G_LOG_DOMAIN "gb-vim"

#include <errno.h>
#include <glib/gi18n.h>
#include <ide.h>

#include "gb-string.h"
#include "gb-vim.h"
#include "gb-widget.h"
#include "gb-workbench.h"

G_DEFINE_QUARK (gb-vim-error-quark, gb_vim_error)

typedef gboolean (*GbVimSetFunc)     (GtkSourceView  *source_view,
                                      const gchar    *key,
                                      const gchar    *value,
                                      GError        **error);
typedef gboolean (*GbVimCommandFunc) (GtkSourceView  *source_view,
                                      const gchar    *command,
                                      const gchar    *options,
                                      GError        **error);

typedef struct
{
  gchar         *name;
  GbVimSetFunc  func;
} GbVimSet;

typedef struct
{
  gchar *name;
  gchar *alias;
} GbVimSetAlias;

typedef struct
{
  gchar             *name;
  GbVimCommandFunc  func;
  gchar            *options_sup;
} GbVimCommand;

static gboolean
int32_parse (gint         *value,
             const gchar  *str,
             gint          lower,
             gint          upper,
             const gchar  *param_name,
             GError      **error)
{
  gint64 v64;
  gchar *v64_str;

  g_assert (value);
  g_assert (str);
  g_assert (lower <= upper);
  g_assert (param_name);

  v64 = g_ascii_strtoll (str, NULL, 10);

  if (((v64 == G_MININT64) || (v64 == G_MAXINT64)) && (errno == ERANGE))
    {
      g_set_error (error,
                   GB_VIM_ERROR,
                   GB_VIM_ERROR_NOT_NUMBER,
                   _("Number required"));
      return FALSE;
    }

  if ((v64 < lower) || (v64 > upper))
    {
      v64_str = g_strdup_printf ("%"G_GINT64_FORMAT, v64);
      g_set_error (error,
                   GB_VIM_ERROR,
                   GB_VIM_ERROR_NUMBER_OUT_OF_RANGE,
                   _("%s is invalid for %s"),
                   v64_str, param_name);
      g_free (v64_str);
      return FALSE;
    }

  *value = v64;

  return TRUE;
}

static gboolean
gb_vim_set_autoindent (GtkSourceView  *source_view,
                       const gchar    *key,
                       const gchar    *value,
                       GError        **error)
{
  g_object_set (source_view, "auto-indent", TRUE, NULL);
  return TRUE;
}


static gboolean
gb_vim_set_expandtab (GtkSourceView  *source_view,
                      const gchar    *key,
                      const gchar    *value,
                      GError        **error)
{
  g_object_set (source_view, "insert-spaces-instead-of-tabs", TRUE, NULL);
  return TRUE;
}

static gboolean
gb_vim_set_filetype (GtkSourceView  *source_view,
                     const gchar    *key,
                     const gchar    *value,
                     GError        **error)
{
  GtkSourceLanguageManager *manager;
  GtkSourceLanguage *language;
  GtkTextBuffer *buffer;

  if (0 == g_strcmp0 (value, "cs"))
    value = "c-sharp";
  else if (0 == g_strcmp0 (value, "xhmtl"))
    value = "html";
  else if (0 == g_strcmp0 (value, "javascript"))
    value = "js";

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));
  manager = gtk_source_language_manager_get_default ();
  language = gtk_source_language_manager_get_language (manager, value);

  if (language == NULL)
    {
      g_set_error (error,
                   GB_VIM_ERROR,
                   GB_VIM_ERROR_UNKNOWN_OPTION,
                   _("Cannot find language '%s'"),
                   value);
      return FALSE;
    }

  g_object_set (buffer, "language", language, NULL);

  return TRUE;
}

static gboolean
gb_vim_set_noautoindent (GtkSourceView  *source_view,
                         const gchar    *key,
                         const gchar    *value,
                         GError        **error)
{
  g_object_set (source_view, "auto-indent", FALSE, NULL);
  return TRUE;
}

static gboolean
gb_vim_set_noexpandtab (GtkSourceView  *source_view,
                        const gchar    *key,
                        const gchar    *value,
                        GError        **error)
{
  g_object_set (source_view, "insert-spaces-instead-of-tabs", FALSE, NULL);
  return TRUE;
}

static gboolean
gb_vim_set_nonumber (GtkSourceView  *source_view,
                     const gchar    *key,
                     const gchar    *value,
                     GError        **error)
{
  g_object_set (source_view, "show-line-numbers", FALSE, NULL);
  return TRUE;
}

static gboolean
gb_vim_set_number (GtkSourceView  *source_view,
                   const gchar    *key,
                   const gchar    *value,
                   GError        **error)
{
  g_object_set (source_view, "show-line-numbers", TRUE, NULL);
  return TRUE;
}

static gboolean
gb_vim_set_scrolloff (GtkSourceView  *source_view,
                      const gchar    *key,
                      const gchar    *value,
                      GError        **error)
{
  gint scroll_offset = 0;

  if (!int32_parse (&scroll_offset, value, 0, G_MAXINT32, "scroll size", error))
    return FALSE;
  if (IDE_IS_SOURCE_VIEW (source_view))
    g_object_set (source_view, "scroll-offset", scroll_offset, NULL);
  return TRUE;
}

static gboolean
gb_vim_set_shiftwidth (GtkSourceView  *source_view,
                       const gchar    *key,
                       const gchar    *value,
                       GError        **error)
{
  gint shiftwidth = 0;

  if (!int32_parse (&shiftwidth, value, 0, G_MAXINT32, "shift width", error))
    return FALSE;

  if (shiftwidth == 0)
    shiftwidth = -1;

  g_object_set (source_view, "indent-width", shiftwidth, NULL);
  return TRUE;
}

static gboolean
gb_vim_set_tabstop (GtkSourceView  *source_view,
                    const gchar    *key,
                    const gchar    *value,
                    GError        **error)
{
  gint tabstop  = 0;

  if (!int32_parse (&tabstop , value, 1, 32, "tab stop", error))
    return FALSE;

  g_object_set (source_view, "tab-width", tabstop, NULL);
  return TRUE;
}

static const GbVimSet vim_sets [] = {
  { "autoindent",    gb_vim_set_autoindent },
  { "expandtab",     gb_vim_set_expandtab },
  { "filetype",      gb_vim_set_filetype },
  { "noautoindent",  gb_vim_set_noautoindent },
  { "noexpandtab",   gb_vim_set_noexpandtab },
  { "nonumber",      gb_vim_set_nonumber },
  { "number",        gb_vim_set_number },
  { "scrolloff",     gb_vim_set_scrolloff },
  { "shiftwidth",    gb_vim_set_shiftwidth },
  { "tabstop",       gb_vim_set_tabstop },
  { NULL }
};

static const GbVimSetAlias vim_set_aliases[] = {
  { "ai",   "autoindent" },
  { "et",   "expandtab" },
  { "ft",   "filetype" },
  { "noet", "noexpandtab" },
  { "nu",   "number" },
  { "noai", "noautoindent" },
  { "nonu", "nonumber" },
  { "so",   "scrolloff" },
  { "sw",   "shiftwidth" },
  { "ts",   "tabstop" },
  { NULL }
};

static const GbVimSet *
lookup_set (const gchar *key)
{
  gsize i;

  g_assert (key);

  for (i = 0; vim_set_aliases [i].name; i++)
    {
      if (g_str_equal (vim_set_aliases [i].name, key))
        {
          key = vim_set_aliases [i].alias;
          break;
        }
    }

  for (i = 0; vim_sets [i].name; i++)
    {
      if (g_str_equal (vim_sets [i].name, key))
        return &vim_sets [i];
    }

  return NULL;
}

static gboolean
gb_vim_command_set (GtkSourceView  *source_view,
                    const gchar    *command,
                    const gchar    *options,
                    GError        **error)
{
  gboolean ret = FALSE;
  gchar **parts;
  gsize i;

  g_assert (GTK_SOURCE_IS_VIEW (source_view));
  g_assert (command);
  g_assert (options);

  parts = g_strsplit (options, " ", 0);

  if (parts [0] == NULL)
    {
      ret = TRUE;
      goto cleanup;
    }

  for (i = 0; parts [i]; i++)
    {
      const GbVimSet *set;
      const gchar *value = "";
      gchar *key = parts [i];
      gchar *tmp;

      for (tmp = key; *tmp; tmp = g_utf8_next_char (tmp))
        {
          if (g_utf8_get_char (tmp) == '=')
            {
              *tmp = '\0';
              value = ++tmp;
              break;
            }
        }

      set = lookup_set (key);

      if (set == NULL)
        {
          g_set_error (error,
                       GB_VIM_ERROR,
                       GB_VIM_ERROR_UNKNOWN_OPTION,
                       _("Unknown option: %s"),
                       key);
          goto cleanup;
        }

      if (!set->func (source_view, key, value, error))
        goto cleanup;
    }

  ret = TRUE;

cleanup:
  g_strfreev (parts);

  return ret;
}

static gboolean
gb_vim_command_colorscheme (GtkSourceView  *source_view,
                            const gchar    *command,
                            const gchar    *options,
                            GError        **error)
{
  GtkSourceStyleSchemeManager *manager;
  GtkSourceStyleScheme *style_scheme;
  GtkTextBuffer *buffer;
  g_autofree gchar *trimmed = NULL;

  trimmed = g_strstrip (g_strdup (options));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));
  manager = gtk_source_style_scheme_manager_get_default ();
  style_scheme = gtk_source_style_scheme_manager_get_scheme (manager, trimmed);

  if (style_scheme == NULL)
    {
      g_set_error (error,
                   GB_VIM_ERROR,
                   GB_VIM_ERROR_UNKNOWN_OPTION,
                   _("Cannot find colorscheme '%s'"),
                   options);
      return FALSE;
    }

  g_object_set (buffer, "style-scheme", style_scheme, NULL);

  return TRUE;
}

static gboolean
gb_vim_command_edit (GtkSourceView  *source_view,
                     const gchar    *command,
                     const gchar    *options,
                     GError        **error)
{
  GbWorkbench *workbench;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;
  GFile *file = NULL;

  if (gb_str_empty0 (options))
    {
      gb_widget_activate_action (GTK_WIDGET (source_view), "workbench", "open", NULL);
      return TRUE;
    }

  if (!(workbench = gb_widget_get_workbench (GTK_WIDGET (source_view))) ||
      !(context = gb_workbench_get_context (workbench)) ||
      !(vcs = ide_context_get_vcs (context)) ||
      !(workdir = ide_vcs_get_working_directory (vcs)))
    {
      g_set_error (error,
                   GB_VIM_ERROR,
                   GB_VIM_ERROR_NOT_SOURCE_VIEW,
                   _("Failed to locate working directory"));
      return FALSE;
    }

  if (g_path_is_absolute (options))
    file = g_file_new_for_path (options);
  else
    file = g_file_get_child (workdir, options);

  gb_workbench_open (workbench, file);

  g_clear_object (&file);

  return TRUE;
}

static gboolean
gb_vim_command_tabe (GtkSourceView  *source_view,
                     const gchar    *command,
                     const gchar    *options,
                     GError        **error)
{
  if (!gb_str_empty0 (options))
    return gb_vim_command_edit (source_view, command, options, error);

  gb_widget_activate_action (GTK_WIDGET (source_view), "workbench", "new-document", NULL);

  return TRUE;
}

static gboolean
gb_vim_command_quit (GtkSourceView  *source_view,
                     const gchar    *command,
                     const gchar    *options,
                     GError        **error)
{
  gb_widget_activate_action (GTK_WIDGET (source_view), "view", "save", NULL);
  gb_widget_activate_action (GTK_WIDGET (source_view), "view", "close", NULL);
  return TRUE;
}

static gboolean
gb_vim_command_split (GtkSourceView  *source_view,
                      const gchar    *command,
                      const gchar    *options,
                      GError        **error)
{
  gb_widget_activate_action (GTK_WIDGET (source_view), "view-stack", "split-down", NULL);
  return TRUE;
}

static gboolean
gb_vim_command_vsplit (GtkSourceView  *source_view,
                       const gchar    *command,
                       const gchar    *options,
                       GError        **error)
{
  gb_widget_activate_action (GTK_WIDGET (source_view), "view-stack", "split-left", NULL);
  return TRUE;
}

static gboolean
gb_vim_command_write (GtkSourceView  *source_view,
                      const gchar    *command,
                      const gchar    *options,
                      GError        **error)
{
  gb_widget_activate_action (GTK_WIDGET (source_view), "view", "save", NULL);
  return TRUE;
}

static gboolean
gb_vim_command_wq (GtkSourceView  *source_view,
                   const gchar    *command,
                   const gchar    *options,
                   GError        **error)
{
  return (gb_vim_command_write (source_view, command, options, error) &&
          gb_vim_command_quit (source_view, command, options, error));
}

static gboolean
gb_vim_command_nohl (GtkSourceView  *source_view,
                     const gchar    *command,
                     const gchar    *options,
                     GError        **error)
{
  if (IDE_IS_SOURCE_VIEW (source_view))
    {
      GtkSourceSearchContext *context = NULL;

      g_object_get (source_view, "search-context", &context, NULL);
      g_object_set (context, "highlight", FALSE, NULL);
      g_clear_object (&context);
    }

  return TRUE;
}

static gboolean
gb_vim_command_make (GtkSourceView  *source_view,
                     const gchar    *command,
                     const gchar    *options,
                     GError        **error)
{
  gb_widget_activate_action (GTK_WIDGET (source_view), "workbench", "build", NULL);
  return TRUE;
}

static gboolean
gb_vim_command_syntax (GtkSourceView  *source_view,
                       const gchar    *command,
                       const gchar    *options,
                       GError        **error)
{
  if (g_str_equal (options, "enable") || g_str_equal (options, "on"))
    g_object_set (source_view, "highlight-syntax", TRUE, NULL);
  else if (g_str_equal (options, "off"))
    g_object_set (source_view, "highlight-syntax", FALSE, NULL);
  else
    {
      g_set_error (error,
                   GB_VIM_ERROR,
                   GB_VIM_ERROR_UNKNOWN_OPTION,
                   _("Invalid :syntax subcommand: %s"),
                   options);
      return FALSE;
    }

  return TRUE;
}

static gboolean
gb_vim_command_sort (GtkSourceView  *source_view,
                     const gchar    *command,
                     const gchar    *options,
                     GError        **error)
{
  if (IDE_IS_SOURCE_VIEW (source_view))
    {
      g_signal_emit_by_name (source_view, "sort", FALSE, FALSE);
      g_signal_emit_by_name (source_view, "clear-selection");
      g_signal_emit_by_name (source_view, "set-mode", NULL,
                             IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT);
    }

  return TRUE;
}

static gboolean
gb_vim_command_bnext (GtkSourceView  *source_view,
                      const gchar    *command,
                      const gchar    *options,
                      GError        **error)
{
  gb_widget_activate_action (GTK_WIDGET (source_view), "view-stack", "next-view", NULL);
  return TRUE;
}

static gboolean
gb_vim_command_bprevious (GtkSourceView  *source_view,
                          const gchar    *command,
                          const gchar    *options,
                          GError        **error)
{
  gb_widget_activate_action (GTK_WIDGET (source_view), "view-stack", "previous-view", NULL);
  return TRUE;
}

static gboolean
gb_vim_command_cnext (GtkSourceView  *source_view,
                      const gchar    *command,
                      const gchar    *options,
                      GError        **error)
{
  if (IDE_IS_SOURCE_VIEW (source_view))
    g_signal_emit_by_name (source_view, "move-error", GTK_DIR_DOWN);
  return TRUE;
}

static gboolean
gb_vim_command_cprevious (GtkSourceView  *source_view,
                          const gchar    *command,
                          const gchar    *options,
                          GError        **error)
{
  if (IDE_IS_SOURCE_VIEW (source_view))
    g_signal_emit_by_name (source_view, "move-error", GTK_DIR_UP);
  return TRUE;
}

static gboolean
gb_vim_command_buffers (GtkSourceView  *source_view,
                        const gchar    *command,
                        const gchar    *options,
                        GError        **error)
{
  gb_widget_activate_action (GTK_WIDGET (source_view), "view-stack", "show-list", NULL);
  return TRUE;
}

static gboolean
gb_vim_jump_to_line (GtkSourceView  *source_view,
                     const gchar    *command,
                     const gchar    *options,
                     GError        **error)
{
  GtkTextBuffer *buffer;
  gboolean extend_selection;
  gint line;

  if (!IDE_IS_SOURCE_VIEW (source_view))
    return TRUE;

  if (!int32_parse (&line, options, 0, G_MAXINT32, "line number", error))
    return FALSE;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));
  extend_selection = gtk_text_buffer_get_has_selection (buffer);
  ide_source_view_set_count (IDE_SOURCE_VIEW (source_view), line);

  g_signal_emit_by_name (source_view,
                         "movement",
                         IDE_SOURCE_VIEW_MOVEMENT_NTH_LINE,
                         extend_selection, TRUE, TRUE);

  g_signal_emit_by_name (source_view, "save-insert-mark");

  return TRUE;
}

static gboolean
gb_vim_command_help (GtkSourceView  *source_view,
                     const gchar    *command,
                     const gchar    *options,
                     GError        **error)
{
  GVariant *param;

  param = g_variant_new_string (options);
  gb_widget_activate_action (GTK_WIDGET (source_view), "workbench", "search-docs", param);
  return TRUE;
}

static gboolean
gb_vim_match_is_selected (GtkTextBuffer *buffer,
                          GtkTextIter   *match_begin,
                          GtkTextIter   *match_end)
{
  GtkTextIter sel_begin;
  GtkTextIter sel_end;

  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (match_begin);
  g_assert (match_end);

  gtk_text_buffer_get_selection_bounds (buffer, &sel_begin, &sel_end);
  gtk_text_iter_order (&sel_begin, &sel_end);

  return ((gtk_text_iter_compare (&sel_begin, match_begin) <= 0) &&
          (gtk_text_iter_compare (&sel_begin, match_end) < 0) &&
          (gtk_text_iter_compare (&sel_end, match_begin) > 0) &&
          (gtk_text_iter_compare (&sel_end, match_end) >= 0));
}

static void
gb_vim_do_search_and_replace (GtkTextBuffer *buffer,
                              GtkTextIter   *begin,
                              GtkTextIter   *end,
                              const gchar   *search_text,
                              const gchar   *replace_text,
                              gboolean       is_global)
{
  GtkSourceSearchContext *search_context;
  GtkSourceSearchSettings *search_settings;
  GtkTextMark *mark;
  GtkTextIter tmp1;
  GtkTextIter tmp2;
  GtkTextIter match_begin;
  GtkTextIter match_end;
  GError *error = NULL;

  g_assert (search_text);
  g_assert (replace_text);
  g_assert ((!begin && !end) || (begin && end));

  search_settings = gtk_source_search_settings_new ();
  search_context = gtk_source_search_context_new (GTK_SOURCE_BUFFER (buffer), search_settings);

  if (!begin)
    {
      gtk_text_buffer_get_start_iter (buffer, &tmp1);
      begin = &tmp1;
    }

  if (!end)
    {
      gtk_text_buffer_get_end_iter (buffer, &tmp2);
      end = &tmp2;
    }

  mark = gtk_text_buffer_create_mark (buffer, NULL, end, FALSE);

  gtk_source_search_settings_set_search_text (search_settings, search_text);
  gtk_source_search_settings_set_case_sensitive (search_settings, TRUE);

  while (gtk_source_search_context_forward (search_context, begin, &match_begin, &match_end))
    {
      if (is_global || gb_vim_match_is_selected (buffer, &match_begin, &match_end))
        {
          GtkTextMark *mark2;

          mark2 = gtk_text_buffer_create_mark (buffer, NULL, &match_end, FALSE);

          if (!gtk_source_search_context_replace (search_context, &match_begin, &match_end,
                                                  replace_text, -1, &error))
            {
              g_warning ("%s", error->message);
              g_clear_error (&error);
              gtk_text_buffer_delete_mark (buffer, mark2);
              break;
            }

          gtk_text_buffer_get_iter_at_mark (buffer, &match_end, mark2);
          gtk_text_buffer_delete_mark (buffer, mark2);
        }

      *begin = match_end;

      gtk_text_buffer_get_iter_at_mark (buffer, end, mark);
    }

  gtk_text_buffer_delete_mark (buffer, mark);

  g_clear_object (&search_settings);
  g_clear_object (&search_context);
}

static gboolean
gb_vim_command_search (GtkSourceView  *source_view,
                       const gchar    *command,
                       const gchar    *options,
                       GError        **error)
{
  GtkTextBuffer *buffer;
  const gchar *search_begin = NULL;
  const gchar *search_end = NULL;
  const gchar *replace_begin = NULL;
  const gchar *replace_end = NULL;
  gchar *search_text = NULL;
  gchar *replace_text = NULL;
  gunichar separator;

  g_assert (g_str_has_prefix (command, "%s") || g_str_has_prefix (command, "s"));

  if (*command == '%')
    command++;
  command++;

  separator = g_utf8_get_char (command);
  if (!separator)
    goto invalid_request;

  search_begin = command = g_utf8_next_char (command);

  for (; *command; command = g_utf8_next_char (command))
    {
      if (*command == '\\')
        {
          command = g_utf8_next_char (command);
          if (!*command)
            goto invalid_request;
          continue;
        }

      if (g_utf8_get_char (command) == separator)
        {
          search_end = command;
          break;
        }
    }

  if (!search_end)
    goto invalid_request;

  replace_begin = command = g_utf8_next_char (command);

  for (; *command; command = g_utf8_next_char (command))
    {
      if (*command == '\\')
        {
          command = g_utf8_next_char (command);
          if (!*command)
            goto invalid_request;
          continue;
        }

      if (g_utf8_get_char (command) == separator)
        {
          replace_end = command;
          break;
        }
    }

  if (!replace_end)
    goto invalid_request;

  command = g_utf8_next_char (command);

  if (*command)
    {
      for (; *command; command++)
        {
          switch (*command)
            {
            case 'g':
              break;

            /* what other options are supported? */
            default:
              break;
            }
        }
    }

  search_text = g_strndup (search_begin, search_end - search_begin);
  replace_text = g_strndup (replace_begin, replace_end - replace_begin);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));

  if (gtk_text_buffer_get_has_selection (buffer))
    {
      GtkTextIter begin;
      GtkTextIter end;

      gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
      gtk_text_iter_order (&begin, &end);
      gb_vim_do_search_and_replace (buffer, &begin, &end, search_text, replace_text, FALSE);
    }
  else
    gb_vim_do_search_and_replace (buffer, NULL, NULL, search_text, replace_text, TRUE);

  g_free (search_text);
  g_free (replace_text);

  return TRUE;

invalid_request:
  g_set_error (error,
               GB_VIM_ERROR,
               GB_VIM_ERROR_UNKNOWN_OPTION,
               _("Invalid search and replace request"));
  return FALSE;
}

static const GbVimCommand vim_commands[] = {
  { "bnext",       gb_vim_command_bnext , NULL},
  { "bprevious",   gb_vim_command_bprevious, NULL },
  { "buffers",     gb_vim_command_buffers, NULL },
  { "ls",          gb_vim_command_buffers, NULL },
  { "cnext",       gb_vim_command_cnext, NULL },
  { "colorscheme", gb_vim_command_colorscheme, NULL },
  { "cprevious",   gb_vim_command_cprevious, NULL },
  { "edit",        gb_vim_command_edit, NULL },
  { "help",        gb_vim_command_help, NULL },
  { "nohl",        gb_vim_command_nohl, NULL },
  { "make",        gb_vim_command_make, NULL },
  { "quit",        gb_vim_command_quit, NULL },
  { "set",         gb_vim_command_set, NULL },
  { "sort",        gb_vim_command_sort, NULL },
  { "split",       gb_vim_command_split, NULL },
  { "syntax",      gb_vim_command_syntax, NULL },
  { "tabe",        gb_vim_command_tabe, NULL },
  { "vsplit",      gb_vim_command_vsplit, NULL },
  { "w",           gb_vim_command_write, NULL },
  { "wq",          gb_vim_command_wq, NULL },
  { "write",       gb_vim_command_write, NULL },
  { NULL }
};

static gboolean
looks_like_search_and_replace (const gchar *line)
{
  g_assert (line);

  if (g_str_has_prefix (line, "%s"))
    return TRUE;
  return *line == 's';
}

static const GbVimCommand *
lookup_command (const gchar *name)
{
  static GbVimCommand line_command = { "__line__", gb_vim_jump_to_line, NULL };
  gint line;
  gsize i;

  g_assert (name);

  for (i = 0; vim_commands [i].name; i++)
    {
      if (g_str_has_prefix (vim_commands [i].name, name))
        return &vim_commands [i];
    }

  if (g_ascii_isdigit (*name) && int32_parse (&line, name, 0, G_MAXINT32, "line", NULL))
  {
    line_command.options_sup = g_strdup (name);
    return &line_command;
  }

  return NULL;
}

gboolean
gb_vim_execute (GtkSourceView  *source_view,
                const gchar    *line,
                GError        **error)
{
  GtkTextBuffer *buffer;
  g_autofree gchar *name_slice = NULL;
  const GbVimCommand *command;
  const gchar *command_name = line;
  const gchar *options;
  g_autofree gchar *all_options = NULL;
  gboolean result;

  g_return_val_if_fail (GTK_SOURCE_IS_VIEW (source_view), FALSE);
  g_return_val_if_fail (line, FALSE);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));

  if (!GTK_SOURCE_IS_BUFFER (buffer))
    {
      g_set_error (error,
                   GB_VIM_ERROR,
                   GB_VIM_ERROR_NOT_SOURCE_VIEW,
                   _("vim mode requires GtkSourceView"));
      return FALSE;
    }

  for (options = line; *options; options = g_utf8_next_char (options))
    {
      gunichar ch;

      ch = g_utf8_get_char (options);

      if (g_unichar_isspace (ch))
        break;
    }

  if (g_unichar_isspace (g_utf8_get_char (options)))
    {
      command_name = name_slice = g_strndup (line, options - line);
      options = g_utf8_next_char (options);
    }

  command = lookup_command (command_name);

  if (command == NULL)
    {
      if (looks_like_search_and_replace (line))
        return gb_vim_command_search (source_view, line, "", error);

      g_set_error (error,
                   GB_VIM_ERROR,
                   GB_VIM_ERROR_NOT_FOUND,
                   _("Not an editor command: %s"),
                   command_name);
      return FALSE;
    }

  if (command->options_sup)
    all_options = g_strconcat (options, " ", command->options_sup, NULL);
  else
    all_options = g_strdup (options);

  result = command->func (source_view, command_name, all_options, error);
  g_free (command->options_sup);

  return result;
}

static gchar *
joinv_and_add (gchar       **parts,
               gsize         len,
               const gchar  *delim,
               const gchar  *str)
{
  GString *gstr;
  gsize i;

  gstr = g_string_new (parts [0]);
  for (i = 1; i < len; i++)
    g_string_append_printf (gstr, "%s%s", delim, parts [i]);
  g_string_append_printf (gstr, "%s%s", delim, str);
  return g_string_free (gstr, FALSE);
}

static void
gb_vim_complete_set (const gchar *line,
                     GPtrArray   *ar)
{
  const gchar *key;
  gchar **parts;
  guint len;
  gsize i;

  parts = g_strsplit (line, " ", 0);
  len = g_strv_length (parts);

  if (len < 2)
    {
      g_strfreev (parts);
      return;
    }

  key = parts [len - 1];

  for (i = 0; vim_sets [i].name; i++)
    {
      if (g_str_has_prefix (vim_sets [i].name, key))
        g_ptr_array_add (ar, joinv_and_add (parts, len - 1, " ", vim_sets [i].name));
    }

  for (i = 0; vim_set_aliases [i].name; i++)
    {
      if (g_str_has_prefix (vim_set_aliases [i].name, key))
        g_ptr_array_add (ar, joinv_and_add (parts, len - 1, " ", vim_set_aliases [i].name));
    }

  g_strfreev (parts);
}

static void
gb_vim_complete_command (const gchar *line,
                         GPtrArray   *ar)
{
  gsize i;

  for (i = 0; vim_commands [i].name; i++)
    {
      if (g_str_has_prefix (vim_commands [i].name, line))
        g_ptr_array_add (ar, g_strdup (vim_commands [i].name));
    }
}

static void
gb_vim_complete_edit_files (GtkSourceView *source_view,
                            const gchar   *command,
                            GPtrArray     *ar,
                            const gchar   *prefix)
{
  GbWorkbench *workbench;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;
  g_autoptr(GFile) child = NULL;
  g_autoptr(GFile) parent = NULL;

  IDE_ENTRY;

  g_assert (command);
  g_assert (ar);
  g_assert (prefix);

  if (!(workbench = gb_widget_get_workbench (GTK_WIDGET (source_view))) ||
      !(context = gb_workbench_get_context (workbench)) ||
      !(vcs = ide_context_get_vcs (context)) ||
      !(workdir = ide_vcs_get_working_directory (vcs)))
    IDE_EXIT;

  child = g_file_get_child (workdir, prefix);

  if (g_file_query_exists (child, NULL))
    {
      if (g_file_query_file_type (child, 0, NULL) == G_FILE_TYPE_DIRECTORY)
        {
          g_autoptr(GFileEnumerator) fe = NULL;
          GFileInfo *descendent;

          if (!g_str_has_suffix (prefix, "/"))
            {
              g_ptr_array_add (ar, g_strdup_printf ("%s %s/", command, prefix));
              IDE_EXIT;
            }

          fe = g_file_enumerate_children (child,
                                          G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL, NULL);

          if (fe == NULL)
            IDE_EXIT;

          while ((descendent = g_file_enumerator_next_file (fe, NULL, NULL)))
            {
              const gchar *name;

              name = g_file_info_get_display_name (descendent);
              g_ptr_array_add (ar, g_strdup_printf ("%s %s%s", command, prefix, name));
              g_object_unref (descendent);
            }

          IDE_EXIT;
        }
    }

  parent = g_file_get_parent (child);

  if (parent != NULL)
    {
      g_autoptr(GFileEnumerator) fe = NULL;
      g_autofree gchar *relpath = NULL;
      GFileInfo *descendent;
      const gchar *slash;

      relpath = g_file_get_relative_path (workdir, parent);

      if (relpath && g_str_has_prefix (relpath, "./"))
        {
          gchar *tmp = relpath;
          relpath = g_strdup (relpath + 2);
          g_free (tmp);
        }

#ifdef IDE_ENABLE_TRACE
      {
        g_autofree gchar *parent_path = g_file_get_path (parent);
        IDE_TRACE_MSG ("parent_path: %s", parent_path);
      }
#endif

      if ((slash = strrchr (prefix, G_DIR_SEPARATOR)))
        prefix = slash + 1;

      fe = g_file_enumerate_children (parent,
                                      G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                      G_FILE_QUERY_INFO_NONE,
                                      NULL, NULL);

      if (fe == NULL)
        IDE_EXIT;

      while ((descendent = g_file_enumerator_next_file (fe, NULL, NULL)))
        {
          const gchar *name;

          name = g_file_info_get_display_name (descendent);

          IDE_TRACE_MSG ("name=%s prefix=%s", name, prefix);

          if (name && g_str_has_prefix (name, prefix))
            {
              gchar *path;

              if (relpath)
                path = g_strdup_printf ("%s %s/%s", command, relpath, name);
              else
                path = g_strdup_printf ("%s %s", command, name);

              IDE_TRACE_MSG ("edit completion: %s", path);

              g_ptr_array_add (ar, path);
            }
          g_object_unref (descendent);
        }

      IDE_EXIT;
    }

  IDE_EXIT;
}

static void
gb_vim_complete_edit (GtkSourceView *source_view,
                      const gchar   *line,
                      GPtrArray     *ar)
{
  gchar **parts;

  parts = g_strsplit (line, " ", 2);
  if (parts [0] == NULL || parts [1] == NULL)
    {
      g_strfreev (parts);
      return;
    }

  gb_vim_complete_edit_files (source_view, parts [0], ar, parts [1]);

  g_strfreev (parts);
}

static void
gb_vim_complete_colorscheme (const gchar *line,
                             GPtrArray   *ar)
{
  GtkSourceStyleSchemeManager *manager;
  const gchar * const *scheme_ids;
  const gchar *tmp;
  g_autofree gchar *prefix = NULL;
  gsize i;

  manager = gtk_source_style_scheme_manager_get_default ();
  scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);

  for (tmp = strchr (line, ' ');
       tmp && *tmp && g_unichar_isspace (g_utf8_get_char (tmp));
       tmp = g_utf8_next_char (tmp))
    {
      /* do nothing */
    }

  if (!tmp)
    return;

  prefix = g_strndup (line, tmp - line);

  for (i = 0; scheme_ids [i]; i++)
    {
      const gchar *scheme_id = scheme_ids [i];

      if (g_str_has_prefix (scheme_id, tmp))
        {
          gchar *item;

          item = g_strdup_printf ("%s%s", prefix, scheme_id);
          IDE_TRACE_MSG ("colorscheme: %s", item);
          g_ptr_array_add (ar, item);
        }
    }
}

gchar **
gb_vim_complete (GtkSourceView *source_view,
                 const gchar   *line)
{
  GPtrArray *ar;

  ar = g_ptr_array_new ();

  if (line != NULL)
    {
      if (g_str_has_prefix (line, "set "))
        gb_vim_complete_set (line, ar);
      else if (g_str_has_prefix (line, "e ") ||
               g_str_has_prefix (line, "edit ") ||
               g_str_has_prefix (line, "tabe "))
        gb_vim_complete_edit (source_view, line, ar);
      else if (g_str_has_prefix (line, "colorscheme "))
        gb_vim_complete_colorscheme (line, ar);
      else
        gb_vim_complete_command (line, ar);
    }

  g_ptr_array_add (ar, NULL);

  return (gchar **)g_ptr_array_free (ar, FALSE);
}
