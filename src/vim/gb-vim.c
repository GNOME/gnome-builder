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

  g_assert (value);
  g_assert (str);
  g_assert_cmpint (lower, <=, upper);
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
      g_set_error (error,
                   GB_VIM_ERROR,
                   GB_VIM_ERROR_NUMBER_OUT_OF_RANGE,
                   _("%"G_GINT64_FORMAT" is invalid for %s"),
                   v64, param_name);
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
  g_print ("disablign line numbers\n");
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

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));
  manager = gtk_source_style_scheme_manager_get_default ();
  style_scheme = gtk_source_style_scheme_manager_get_scheme (manager, options);

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
  return TRUE;
}

static gboolean
gb_vim_command_vsplit (GtkSourceView  *source_view,
                       const gchar    *command,
                       const gchar    *options,
                       GError        **error)
{
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
                   _("Invalid :syntax subcommand : %s"),
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

static const GbVimCommand vim_commands[] = {
  { "colorscheme", gb_vim_command_colorscheme },
  { "edit",        gb_vim_command_edit },
  { "nohl",        gb_vim_command_nohl },
  { "quit",        gb_vim_command_quit },
  { "set",         gb_vim_command_set },
  { "sort",        gb_vim_command_sort },
  { "split",       gb_vim_command_split },
  { "syntax",      gb_vim_command_syntax },
  { "vsplit",      gb_vim_command_vsplit },
  { "w",           gb_vim_command_write },
  { "wq",          gb_vim_command_wq },
  { "write",       gb_vim_command_write },
  { NULL }
};

static const GbVimCommand *
lookup_command (const gchar *name)
{
  gsize i;

  g_assert (name);

  for (i = 0; vim_commands [i].name; i++)
    {
      if (g_str_has_prefix (vim_commands [i].name, name))
        return &vim_commands [i];
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
      g_set_error (error,
                   GB_VIM_ERROR,
                   GB_VIM_ERROR_NOT_FOUND,
                   _("Not an editor command: %s"),
                   command_name);
      return FALSE;
    }

  return command->func (source_view, command_name, options, error);
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
      else
        gb_vim_complete_command (line, ar);
    }

  g_ptr_array_add (ar, NULL);

  return (gchar **)g_ptr_array_free (ar, FALSE);
}
