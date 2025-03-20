/*
 * ide-terminal.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <adwaita.h>

#include <glib/gi18n.h>

#include <libide-gui.h>
#include <libide-sourceview.h>

#include "ide-terminal.h"

#define VTE_PCRE2_UCP 0x00020000u
#define VTE_PCRE2_MULTILINE 0x00000400u
#define VTE_PCRE2_CASELESS 0x00000008u

#define SIZE_DISMISS_TIMEOUT_MSEC 1000
#define URL_MATCH_CURSOR_NAME "pointer"

#define DROP_REQUEST_PRIORITY               G_PRIORITY_DEFAULT
#define APPLICATION_VND_PORTAL_FILETRANSFER "application/vnd.portal.filetransfer"
#define APPLICATION_VND_PORTAL_FILES        "application/vnd.portal.files"
#define TEXT_X_MOZ_URL                      "text/x-moz-url"
#define TEXT_URI_LIST                       "text/uri-list"
#define FILENAME_PLUS_LOCATION              "(?<filename>[[:alnum:]\\+\\-\\.\\/_]+):(?<line>\\d+):(?<column>\\d+)"

typedef struct
{
  int line;
  int column;
} Position;

typedef struct
{
  IdeTerminalPalette *palette;
  char               *url;

  GtkPopover         *popover;
  GMenu              *terminal_menu;
  GtkWidget          *drop_highlight;
  GtkDropTargetAsync *drop_target;
  GtkRevealer        *size_revealer;
  GtkLabel           *size_label;

  guint               size_dismiss_source;
} IdeTerminalPrivate;

enum {
  PROP_0,
  PROP_PALETTE,
  N_PROPS
};

enum {
  MATCH_CLICKED,
  N_SIGNALS
};

G_DEFINE_TYPE_WITH_PRIVATE (IdeTerminal, ide_terminal, VTE_TYPE_TERMINAL)

static GParamSpec *properties [N_PROPS];
static guint signals[N_SIGNALS];
static const char * const builtin_dingus[] = {
  "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?",
  "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"]",
  FILENAME_PLUS_LOCATION,
};
static VteRegex *builtin_dingus_regex[G_N_ELEMENTS(builtin_dingus)];
static GRegex *filename_regex;
static GSettings *settings;

static void
ide_terminal_update_colors (IdeTerminal *self)
{
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);
  g_autoptr(IdeTerminalPalette) palette = NULL;
  const IdeTerminalPaletteFace *face;
  GtkSourceStyleSchemeManager *style_scheme_manager;
  GtkSourceStyleScheme *scheme;
  AdwStyleManager *style_manager;
  const char *style_scheme;
  const char *palette_name = NULL;
  gboolean dark;

  g_assert (IDE_IS_TERMINAL (self));

  style_manager = adw_style_manager_get_default ();
  dark = adw_style_manager_get_dark (style_manager);

  style_scheme = ide_application_get_style_scheme (IDE_APPLICATION_DEFAULT);
  style_scheme_manager = gtk_source_style_scheme_manager_get_default ();
  scheme = gtk_source_style_scheme_manager_get_scheme (style_scheme_manager, style_scheme);

  if (scheme != NULL)
    {
      dark = ide_source_style_scheme_is_dark (scheme);
      palette_name = gtk_source_style_scheme_get_metadata (scheme, "terminal-palette");
    }

  if (palette_name == NULL)
    palette_name = style_scheme;

  if (!g_set_object (&palette, priv->palette))
    {
      /* Special case solarized which doesn't have metadata
       * upstream to work with.
       */
      if (g_str_has_prefix (style_scheme, "solarized"))
        palette_name = "solarized";
      palette = ide_terminal_palette_new_from_name (palette_name);
    }

  face = ide_terminal_palette_get_face (palette, dark);

  vte_terminal_set_colors (VTE_TERMINAL (self),
                           &face->foreground,
                           &face->background,
                           &face->indexed[0],
                           G_N_ELEMENTS (face->indexed));

  if (face->cursor.alpha > 0)
    vte_terminal_set_color_cursor (VTE_TERMINAL (self), &face->cursor);
  else
    vte_terminal_set_color_cursor (VTE_TERMINAL (self), &face->foreground);

  vte_terminal_set_color_cursor_foreground (VTE_TERMINAL (self), &face->background);
}

static void
ide_terminal_toast (IdeTerminal *self,
                    int             timeout,
                    const char     *title)
{
  GtkWidget *overlay = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_TOAST_OVERLAY);
  AdwToast *toast;

  if (overlay == NULL)
    return;

  toast = g_object_new (ADW_TYPE_TOAST,
                        "title", title,
                        "timeout", timeout,
                        NULL);
  adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (overlay), toast);
}

static gboolean
ide_terminal_is_active (IdeTerminal *self)
{
  AdwTabView *tab_view;
  IdePage *page;

  g_assert (IDE_IS_TERMINAL (self));

  if ((tab_view = ADW_TAB_VIEW (gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_TAB_VIEW))) &&
      (page = IDE_PAGE (gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_PAGE))))
    return GTK_WIDGET (page) == adw_tab_page_get_child (adw_tab_view_get_selected_page (tab_view));

  return FALSE;
}

static gboolean
clear_url_actions_cb (gpointer data)
{
  IdeTerminal *self = data;

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "clipboard.copy-link", FALSE);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "terminal.open-link", FALSE);

  return G_SOURCE_REMOVE;
}

static void
ide_terminal_update_clipboard_actions (IdeTerminal *self)
{
  GdkClipboard *clipboard;
  gboolean can_paste;
  gboolean has_selection;

  g_assert (IDE_IS_TERMINAL (self));

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self));
  can_paste = gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (clipboard), G_TYPE_STRING);
  has_selection = vte_terminal_get_has_selection (VTE_TERMINAL (self));

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "clipboard.copy", has_selection);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "clipboard.paste", can_paste);
}

static void
ide_terminal_update_url_actions (IdeTerminal *self,
                                 double          x,
                                 double          y)
{
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);
  g_autofree char *pattern = NULL;
  int tag = 0;

  g_assert (IDE_IS_TERMINAL (self));

  pattern = vte_terminal_check_match_at (VTE_TERMINAL (self), x, y, &tag);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "clipboard.copy-link", pattern != NULL);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "terminal.open-link", pattern != NULL);

  g_set_str (&priv->url, pattern);
}

static void
ide_terminal_popover_closed_cb (IdeTerminal *self,
                                GtkPopover     *popover)
{
  g_assert (IDE_IS_TERMINAL (self));
  g_assert (GTK_IS_POPOVER (popover));

  g_idle_add_full (G_PRIORITY_LOW,
                   clear_url_actions_cb,
                   g_object_ref (self),
                   g_object_unref);
}

static void
ide_terminal_open_link_resolve_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeWorkbench *workbench = (IdeWorkbench *)object;
  g_autoptr(GFile) file = NULL;
  Position *pos = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (G_IS_ASYNC_RESULT (result));

  if ((file = ide_workbench_resolve_file_finish (workbench, result, NULL)))
    ide_workbench_open_at_async (workbench,
                                 file,
                                 "editor",
                                 pos->line,
                                 pos->column,
                                 IDE_BUFFER_OPEN_FLAGS_NONE,
                                 NULL, NULL, NULL, NULL);

  g_slice_free (Position, pos);
}

static void
ide_terminal_open_url (IdeTerminal *self,
                       const char  *url)
{
  g_autoptr(GMatchInfo) match = NULL;
  g_autofree char *filename = NULL;
  g_autofree char *line = NULL;
  g_autofree char *column = NULL;

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (url != NULL);

  if (g_regex_match (filename_regex, url, 0, &match) &&
      (filename = g_match_info_fetch (match, 1)) &&
      (line = g_match_info_fetch (match, 2)) &&
      (column = g_match_info_fetch (match, 3)))
    {
      IdeWorkbench *workbench = ide_widget_get_workbench (GTK_WIDGET (self));
      gint64 lineno = g_ascii_strtoull (line, NULL, 10);
      gint64 columnno = g_ascii_strtoull (column, NULL, 10);
      Position pos = {
        MAX (lineno, 1) - 1,
        MAX (columnno, 1) - 1,
      };

      ide_workbench_resolve_file_async (workbench,
                                        filename,
                                        NULL,
                                        ide_terminal_open_link_resolve_cb,
                                        g_slice_dup (Position, &pos));
    }
  else
    {
      g_autoptr(GtkUriLauncher) launcher = gtk_uri_launcher_new (url);
      GtkWindow *window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));

      gtk_uri_launcher_launch (launcher, window, NULL, NULL, NULL);
    }
}

static gboolean
ide_terminal_match_clicked (IdeTerminal  *self,
                            double           x,
                            double           y,
                            int              button,
                            GdkModifierType  state,
                            const char      *match)
{
  gboolean ret = FALSE;

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (match != NULL);

  g_signal_emit (self, signals[MATCH_CLICKED], 0, x, y, button, state, match, &ret);

  /* Currently we only match URLs, but if we change that, we would need to
   * update this to check for dingus/filename/etc.
   */
  if (!ret)
    ide_terminal_open_url (self, match);

  return ret;
}

static void
ide_terminal_popup (IdeTerminal *self,
                    double          x,
                    double          y)
{
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);

  g_assert (IDE_IS_TERMINAL (self));

  ide_terminal_update_clipboard_actions (self);
  ide_terminal_update_url_actions (self, x, y);

  if (priv->popover == NULL)
    {
      priv->popover = GTK_POPOVER (gtk_popover_menu_new_from_model (G_MENU_MODEL (priv->terminal_menu)));

      gtk_popover_set_has_arrow (priv->popover, FALSE);

      if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
        gtk_widget_set_halign (GTK_WIDGET (priv->popover), GTK_ALIGN_END);
      else
        gtk_widget_set_halign (GTK_WIDGET (priv->popover), GTK_ALIGN_START);

      gtk_widget_set_parent (GTK_WIDGET (priv->popover), GTK_WIDGET (self));

      g_signal_connect_object (priv->popover,
                               "closed",
                               G_CALLBACK (ide_terminal_popover_closed_cb),
                               self,
                               G_CONNECT_SWAPPED);
    }

  gtk_popover_set_pointing_to (priv->popover,
                               &(GdkRectangle) { x, y, 1, 1 });

  gtk_popover_popup (priv->popover);
}

static void
ide_terminal_bubble_click_pressed_cb (IdeTerminal  *self,
                                      int              n_press,
                                      double           x,
                                      double           y,
                                      GtkGestureClick *click)
{
  g_assert (IDE_IS_TERMINAL (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  if (n_press == 1)
    {
      GdkModifierType state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (click));
      int button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click));

      if (button == 3)
        {
          if (!(state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK)) ||
              !(state & (GDK_CONTROL_MASK | GDK_ALT_MASK)))
            {
              ide_terminal_popup (self, x, y);
              gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
              return;
            }
        }
    }

  gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_DENIED);
}

static void
ide_terminal_capture_click_pressed_cb (IdeTerminal  *self,
                                       int              n_press,
                                       double           x,
                                       double           y,
                                       GtkGestureClick *click)
{
  g_autofree char *hyperlink = NULL;
  g_autofree char *match = NULL;
  GdkModifierType state;
  gboolean handled = FALSE;
  GdkEvent *event;
  int button;
  int tag = 0;

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (click));
  state = gdk_event_get_modifier_state (event) & gtk_accelerator_get_default_mod_mask ();
  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click));

  hyperlink = vte_terminal_check_hyperlink_at (VTE_TERMINAL (self), x, y);
  match = vte_terminal_check_match_at (VTE_TERMINAL (self), x, y, &tag);

  if (n_press == 1 &&
      !handled &&
      (button == 1 || button == 2) &&
      (state & GDK_CONTROL_MASK))
    {
      if (hyperlink != NULL)
        handled = ide_terminal_match_clicked (self, x, y, button, state, hyperlink);
      else if (match != NULL)
        handled = ide_terminal_match_clicked (self, x, y, button, state, match);
    }

  if (handled)
    gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
  else
    gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_DENIED);
}

static gboolean
ide_terminal_capture_key_pressed_cb (IdeTerminal     *self,
                                     guint               keyval,
                                     guint               keycode,
                                     GdkModifierType     state,
                                     GtkEventController *controller)
{
  GtkScrolledWindow *scroller;
  GtkAdjustment *adjustment;
  double upper;
  double value;
  double page_size;
  GdkEvent *event;

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_KEY (controller));


  /* HACK:
   *
   * This hack works around the fact that GtkScrolledWindow will
   * attempt to continue a kinetic scroll even though VteTerminal will
   * adjust the GtkAdjustment:value to the bottom of the view when
   * scroll-on-keystroke is enabled.
   *
   * This is managed by clearing and resetting the kinetic scrolling
   * property as that will clear any pending kinetic scrolling attempt.
   */

  /* Make sure the property is even enabled first */
  if (!vte_terminal_get_scroll_on_keystroke (VTE_TERMINAL (self)))
    return FALSE;

  /* Check all of the input keyvals which are just modifiers and
   * leave those alone until an input key is pressed.
   */
  event = gtk_event_controller_get_current_event (controller);
  if (gdk_key_event_is_modifier (event))
    return FALSE;

  /* Find our scrolled window and see if kinetic strolling is even enabled */
  scroller = GTK_SCROLLED_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_SCROLLED_WINDOW));
  if (!gtk_scrolled_window_get_kinetic_scrolling (scroller))
    return FALSE;

  /* Tweaking the property is somewhat expensive, so make sure we're not
   * already at the bottom of the visible area before tweaking.
   */
  adjustment = gtk_scrolled_window_get_vadjustment (scroller);
  upper = gtk_adjustment_get_upper (adjustment);
  value = gtk_adjustment_get_value (adjustment);
  page_size = gtk_adjustment_get_page_size (adjustment);
  if (upper - page_size > value)
    {
      gtk_scrolled_window_set_kinetic_scrolling (scroller, FALSE);
      gtk_scrolled_window_set_kinetic_scrolling (scroller, TRUE);
    }

  return FALSE;
}

static void
reset_action (GtkWidget  *widget,
              const char *action_name,
              GVariant   *param)
{
  g_assert (IDE_IS_TERMINAL (widget));
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_BOOLEAN));

  vte_terminal_reset (VTE_TERMINAL (widget), TRUE, g_variant_get_boolean (param));
}

static void
select_all_action (GtkWidget  *widget,
                   const char *action_name,
                   GVariant   *param)
{
  g_assert (IDE_IS_TERMINAL (widget));
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_BOOLEAN));

  if (g_variant_get_boolean (param))
    vte_terminal_select_all (VTE_TERMINAL (widget));
  else
    vte_terminal_unselect_all (VTE_TERMINAL (widget));
}

static void
copy_clipboard_action (GtkWidget  *widget,
                       const char *action_name,
                       GVariant   *param)
{
  IdeTerminal *self = IDE_TERMINAL (widget);
  GdkClipboard *clipboard = gtk_widget_get_clipboard (widget);
  g_autofree char *text = vte_terminal_get_text_selected (VTE_TERMINAL (widget), VTE_FORMAT_TEXT);

  if (text && text[0] != 0)
    {
      gdk_clipboard_set_text (clipboard, text);
      ide_terminal_toast (self, 1, _("Copied to clipboard"));
    }
}

static void
paste_clipboard_action (GtkWidget  *widget,
                        const char *action_name,
                        GVariant   *param)
{
  g_assert (VTE_IS_TERMINAL (widget));

  vte_terminal_paste_clipboard (VTE_TERMINAL (widget));
}

static void
ide_terminal_selection_changed (VteTerminal *terminal)
{
  ide_terminal_update_clipboard_actions (IDE_TERMINAL (terminal));
}

static void
copy_link_address_action (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *param)
{
  IdeTerminal *self = (IdeTerminal *)widget;
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);

  g_assert (IDE_IS_TERMINAL (self));

  if (priv->url && priv->url[0])
    {
      gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (self)), priv->url);
      ide_terminal_toast (self, 1, _("Copied to clipboard"));
    }
}

static void
open_link_action (GtkWidget  *widget,
                  const char *action_name,
                  GVariant   *param)
{
  IdeTerminal *self = (IdeTerminal *)widget;
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);

  g_assert (IDE_IS_TERMINAL (self));

  if (priv->url == NULL || priv->url[0] == 0)
    return;

  ide_terminal_open_url (self, priv->url);
}

typedef struct {
  IdeTerminal *terminal;
  GdkDrop *drop;
  GList *files;
  const char *mime_type;
} TextUriList;

static void
text_uri_list_free (TextUriList *uri_list)
{
  g_clear_object (&uri_list->terminal);
  g_clear_object (&uri_list->drop);
  g_clear_list (&uri_list->files, g_object_unref);
  uri_list->mime_type = NULL;
  g_free (uri_list);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (TextUriList, text_uri_list_free)

static void
ide_terminal_drop_file_list (IdeTerminal *self,
                             const GList    *files)
{
  g_autoptr(GString) string = NULL;

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (files == NULL || G_IS_FILE (files->data));

  string = g_string_new (NULL);

  for (const GList *iter = files; iter; iter = iter->next)
    {
      GFile *file = G_FILE (iter->data);

      if (g_file_is_native (file))
        {
          g_autofree char *quoted = g_shell_quote (g_file_peek_path (file));

          g_string_append (string, quoted);
          g_string_append_c (string, ' ');
        }
      else
        {
          g_autofree char *uri = g_file_get_uri (file);
          g_autofree char *quoted = g_shell_quote (uri);

          g_string_append (string, quoted);
          g_string_append_c (string, ' ');
        }
    }

  if (string->len > 0)
    vte_terminal_paste_text (VTE_TERMINAL (self), string->str);
}

static void
ide_terminal_drop_uri_list_line_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GDataInputStream *line_reader = G_DATA_INPUT_STREAM (object);
  g_autoptr(TextUriList) state = (TextUriList*)user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *line = NULL;
  gsize len = 0;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (IDE_IS_TERMINAL (state->terminal));
  g_assert (GDK_IS_DROP (state->drop));

  line = g_data_input_stream_read_line_finish_utf8 (line_reader, result, &len, &error);

  if (error != NULL)
    {
      g_debug ("Failed to receive '%s': %s", state->mime_type, error->message);
      gdk_drop_finish (state->drop, 0);
      return;
    }

  if (line != NULL && line[0] != 0 && line[0] != '#')
    {
      GFile *file = g_file_new_for_uri (line);

      if (file != NULL)
        state->files = g_list_append (state->files, file);
    }

  if (line == NULL || g_strcmp0 (state->mime_type, TEXT_X_MOZ_URL) == 0)
    {
      ide_terminal_drop_file_list (state->terminal, state->files);
      gdk_drop_finish (state->drop, GDK_ACTION_COPY);
      return;
    }

  g_data_input_stream_read_line_async (line_reader,
                                       DROP_REQUEST_PRIORITY,
                                       NULL,
                                       ide_terminal_drop_uri_list_line_cb,
                                       g_steal_pointer (&state));
}

static void
ide_terminal_drop_uri_list_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GdkDrop *drop = (GdkDrop *)object;
  g_autoptr(IdeTerminal) self = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GInputStream) stream = NULL;
  g_autoptr(GDataInputStream) line_reader = NULL;
  g_autoptr(TextUriList) state = NULL;
  const char *mime_type = NULL;

  g_assert (GDK_IS_DROP (drop));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TERMINAL (self));

  if (!(stream = gdk_drop_read_finish (drop, result, &mime_type, &error)))
    {
      g_debug ("Failed to receive text/uri-list offer: %s", error->message);
      gdk_drop_finish (drop, 0);
      return;
    }

  g_assert (g_strcmp0 (mime_type, TEXT_URI_LIST) == 0);
  g_assert (G_IS_INPUT_STREAM (stream));

  line_reader = g_data_input_stream_new (stream);
  g_data_input_stream_set_newline_type (line_reader,
                                        G_DATA_STREAM_NEWLINE_TYPE_CR_LF);

  state = g_new0 (TextUriList, 1);
  state->terminal = g_object_ref (self);
  state->drop = g_object_ref (drop);
  state->mime_type = g_intern_string (mime_type);

  g_data_input_stream_read_line_async (line_reader,
                                       DROP_REQUEST_PRIORITY,
                                       NULL,
                                       ide_terminal_drop_uri_list_line_cb,
                                       g_steal_pointer (&state));
}

static void
ide_terminal_drop_file_list_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GdkDrop *drop = (GdkDrop *)object;
  g_autoptr(IdeTerminal) self = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;
  const GList *file_list;

  g_assert (GDK_IS_DROP (drop));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TERMINAL (self));

  if (!(value = gdk_drop_read_value_finish (drop, result, &error)))
    {
      g_debug ("Failed to receive file-list offer: %s", error->message);

      /* If the user dragged a directory from Nautilus or another
       * new-style application, a portal request would be made. But
       * GTK won't be able to open the directory so the request for
       * APPLICATION_VND_PORTAL_FILETRANSFER will fail. Fallback to
       * opening the request via TEXT_URI_LIST gracefully.
       */
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) ||
          g_error_matches (error, G_IO_ERROR, G_DBUS_ERROR_ACCESS_DENIED))
        gdk_drop_read_async (drop,
                             (const char **)(const char * const[]){TEXT_URI_LIST, NULL},
                             DROP_REQUEST_PRIORITY,
                             NULL,
                             ide_terminal_drop_uri_list_cb,
                             g_object_ref (self));
      else
        gdk_drop_finish (drop, 0);

      return;
    }

  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST));

  file_list = (const GList *)g_value_get_boxed (value);
  ide_terminal_drop_file_list (self, file_list);
  gdk_drop_finish (drop, GDK_ACTION_COPY);
}

static void
ide_terminal_drop_string_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  GdkDrop *drop = (GdkDrop *)object;
  g_autoptr(IdeTerminal) self = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;
  const char *string;

  g_assert (GDK_IS_DROP (drop));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TERMINAL (self));

  if (!(value = gdk_drop_read_value_finish (drop, result, &error)))
    {
      gdk_drop_finish (drop, 0);
      return;
    }

  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS_STRING (value));

  string = g_value_get_string (value);

  if (string != NULL && string[0] != 0)
    vte_terminal_paste_text (VTE_TERMINAL (self), string);

  gdk_drop_finish (drop, GDK_ACTION_COPY);
}

static void
ide_terminal_drop_moz_url_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GdkDrop *drop = (GdkDrop *)object;
  g_autoptr(IdeTerminal) self = user_data;
  g_autoptr(GCharsetConverter) converter = NULL;
  g_autoptr(GDataInputStream) line_reader = NULL;
  g_autoptr(GInputStream) converter_stream = NULL;
  g_autoptr(GInputStream) stream = NULL;
  g_autoptr(GError) error = NULL;
  const char *mime_type = NULL;
  TextUriList *state;

  g_assert (GDK_IS_DROP (drop));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TERMINAL (self));

  if (!(stream = gdk_drop_read_finish (drop, result, &mime_type, &error))) {
    gdk_drop_finish (drop, 0);
    return;
  }

  g_assert (G_IS_INPUT_STREAM (stream));

  if (!(converter = g_charset_converter_new ("UTF-8", "UCS-2", &error))) {
    g_debug ("Failed to create UTF-8 decoder: %s", error->message);
    gdk_drop_finish (drop, 0);
    return;
  }

  /* TEXT_X_MOZ_URL is in UCS-2 so convert it to UTF-8.
   *
   * The data is expected to be URL, a \n, then the title of the web page.
   *
   * However, some applications (e.g. dolphin) delimit with a \r\n (see
   * issue#293) so handle that generically with the line reader.
   */
  converter_stream = g_converter_input_stream_new (stream, G_CONVERTER (converter));
  line_reader = g_data_input_stream_new (converter_stream);
  g_data_input_stream_set_newline_type (line_reader,
                                        G_DATA_STREAM_NEWLINE_TYPE_ANY);

  state = g_new0 (TextUriList, 1);
  state->terminal = g_object_ref (self);
  state->drop = g_object_ref (drop);
  state->mime_type = g_intern_string (TEXT_X_MOZ_URL);

  g_data_input_stream_read_line_async (line_reader,
                                       DROP_REQUEST_PRIORITY,
                                       NULL,
                                       ide_terminal_drop_uri_list_line_cb,
                                       g_steal_pointer (&state));
}

static gboolean
ide_terminal_drop_target_drop (IdeTerminal     *self,
                               GdkDrop            *drop,
                               double              x,
                               double              y,
                               GtkDropTargetAsync *drop_target)
{
  GdkContentFormats *formats;

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (GDK_IS_DROP (drop));
  g_assert (GTK_IS_DROP_TARGET_ASYNC (drop_target));

  formats = gdk_drop_get_formats (drop);

  if (gdk_content_formats_contain_gtype (formats, GDK_TYPE_FILE_LIST) ||
      gdk_content_formats_contain_gtype (formats, G_TYPE_FILE) ||
      gdk_content_formats_contain_mime_type (formats, TEXT_URI_LIST) ||
      gdk_content_formats_contain_mime_type (formats, APPLICATION_VND_PORTAL_FILETRANSFER) ||
      gdk_content_formats_contain_mime_type (formats, APPLICATION_VND_PORTAL_FILES)) {
    gdk_drop_read_value_async (drop,
                               GDK_TYPE_FILE_LIST,
                               DROP_REQUEST_PRIORITY,
                               NULL,
                               ide_terminal_drop_file_list_cb,
                               g_object_ref (self));
    return TRUE;
  } else if (gdk_content_formats_contain_mime_type (formats, TEXT_X_MOZ_URL)) {
    gdk_drop_read_async (drop,
                         (const char **)(const char * const []){TEXT_X_MOZ_URL, NULL},
                         DROP_REQUEST_PRIORITY,
                         NULL,
                         ide_terminal_drop_moz_url_cb,
                         g_object_ref (self));
    return TRUE;
  } else if (gdk_content_formats_contain_gtype (formats, G_TYPE_STRING)) {
    gdk_drop_read_value_async (drop,
                               G_TYPE_STRING,
                               DROP_REQUEST_PRIORITY,
                               NULL,
                               ide_terminal_drop_string_cb,
                               g_object_ref (self));
    return TRUE;
  }

  return FALSE;
}

static GdkDragAction
ide_terminal_drop_target_drag_enter (IdeTerminal     *self,
                                     GdkDrop            *drop,
                                     double              x,
                                     double              y,
                                     GtkDropTargetAsync *drop_target)
{
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (GTK_IS_DROP_TARGET_ASYNC (drop_target));

  gtk_widget_set_visible (priv->drop_highlight, TRUE);

  return GDK_ACTION_COPY;
}

static void
ide_terminal_drop_target_drag_leave (IdeTerminal     *self,
                                     GdkDrop            *drop,
                                     GtkDropTargetAsync *drop_target)
{
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (GTK_IS_DROP_TARGET_ASYNC (drop_target));

  gtk_widget_set_visible (priv->drop_highlight, FALSE);
}

static void
ide_terminal_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  IdeTerminal *self = IDE_TERMINAL (widget);
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);
  int min_revealer;
  int nat_revealer;

  GTK_WIDGET_CLASS (ide_terminal_parent_class)->measure (widget,
                                                            orientation,
                                                            for_size,
                                                            minimum,
                                                            natural,
                                                            minimum_baseline,
                                                            natural_baseline);

  gtk_widget_measure (GTK_WIDGET (priv->size_revealer),
                      orientation, for_size,
                      &min_revealer, &nat_revealer, NULL, NULL);

  *minimum = MAX (*minimum, min_revealer);
  *natural = MAX (*natural, nat_revealer);
}

static gboolean
dismiss_size_label_cb (gpointer user_data)
{
  IdeTerminal *self = user_data;
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);

  g_assert (IDE_IS_TERMINAL (self));

  gtk_revealer_set_reveal_child (priv->size_revealer, FALSE);

  priv->size_dismiss_source = 0;

  return G_SOURCE_REMOVE;
}

static void
ide_terminal_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
  IdeTerminal *self = IDE_TERMINAL (widget);
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);
  GtkRequisition min;
  GtkAllocation revealer_alloc, dnd_alloc;
  GtkBorder padding;
  GtkRoot *root;
  int prev_column_count, column_count;
  int prev_row_count, row_count;

  g_assert (IDE_IS_TERMINAL (self));

  prev_column_count = vte_terminal_get_column_count (VTE_TERMINAL (self));
  prev_row_count = vte_terminal_get_row_count (VTE_TERMINAL (self));

  GTK_WIDGET_CLASS (ide_terminal_parent_class)->size_allocate (widget, width, height, baseline);

  column_count = vte_terminal_get_column_count (VTE_TERMINAL (self));
  row_count = vte_terminal_get_row_count (VTE_TERMINAL (self));

  root = gtk_widget_get_root (widget);

  if (ide_terminal_is_active (self) &&
      GTK_IS_WINDOW (root) &&
      !gtk_window_is_maximized (GTK_WINDOW (root)) &&
      !gtk_window_is_fullscreen (GTK_WINDOW (root)) &&
      (prev_column_count != column_count || prev_row_count != row_count))
    {
      char format[32];

      g_snprintf (format, sizeof format, "%ld × %ld",
                  vte_terminal_get_column_count (VTE_TERMINAL (self)),
                  vte_terminal_get_row_count (VTE_TERMINAL (self)));
      gtk_label_set_label (priv->size_label, format);

      gtk_revealer_set_reveal_child (priv->size_revealer, TRUE);

      g_clear_handle_id (&priv->size_dismiss_source, g_source_remove);
      priv->size_dismiss_source = g_timeout_add (SIZE_DISMISS_TIMEOUT_MSEC,
                                                 dismiss_size_label_cb,
                                                 self);
    }
  else if (gtk_window_is_maximized (GTK_WINDOW (root)) ||
           gtk_window_is_fullscreen (GTK_WINDOW (root)))
    {
      g_clear_handle_id (&priv->size_dismiss_source, g_source_remove);
      gtk_revealer_set_reveal_child (priv->size_revealer, FALSE);
    }

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_style_context_get_padding (gtk_widget_get_style_context (widget), &padding);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  gtk_widget_get_preferred_size (GTK_WIDGET (priv->size_revealer), &min, NULL);
  revealer_alloc.x = width + padding.right - min.width;
  revealer_alloc.y = height - min.height;
  revealer_alloc.width = min.width;
  revealer_alloc.height = min.height;
  gtk_widget_size_allocate (GTK_WIDGET (priv->size_revealer), &revealer_alloc, -1);

  gtk_widget_get_preferred_size (GTK_WIDGET (priv->drop_highlight), &min, NULL);
  dnd_alloc.x = -padding.left + 1;
  dnd_alloc.y = 1;
  dnd_alloc.width = padding.left - 1 + width + padding.right - 1;
  dnd_alloc.height = height - 2;
  gtk_widget_size_allocate (GTK_WIDGET (priv->drop_highlight), &dnd_alloc, -1);

  if (priv->popover)
    gtk_popover_present (priv->popover);
}

/*
 * ide_terminal_rewrite_snapshot:
 *
 * This function will chain up to the parent VteTerminal to snapshot
 * the terminal. However, afterwards, it rewrites the snapshot to
 * both optimize a large window draw (by removing the color node
 * similar to what vte_terminal_set_clear_background() would do) as
 * well as removing the toplevel clip node.
 *
 * By doing so, we allow our IdeTerminal widget to have padding
 * in the normal case (so that it fits rounded corners well) but also
 * allow the content to reach the top and bottom when scrolling.
 */
static void
ide_terminal_rewrite_snapshot (GtkWidget   *widget,
                               GtkSnapshot *snapshot)
{
  g_autoptr(GtkSnapshot) alternate = NULL;
  g_autoptr(GskRenderNode) root = NULL;
  g_autoptr(GPtrArray) children = NULL;
  gboolean dropped_bg = FALSE;

  g_assert (GTK_IS_SNAPSHOT (snapshot));

  alternate = gtk_snapshot_new ();
  children = g_ptr_array_new ();

  GTK_WIDGET_CLASS (ide_terminal_parent_class)->snapshot (widget, alternate);

  if (!(root = gtk_snapshot_free_to_node (g_steal_pointer (&alternate))))
    return;

  if (gsk_render_node_get_node_type (root) == GSK_CONTAINER_NODE)
    {
      guint n_children = gsk_container_node_get_n_children (root);

      for (guint i = 0; i < n_children; i++)
        {
          GskRenderNode *node = gsk_container_node_get_child (root, i);
          GskRenderNodeType node_type = gsk_render_node_get_node_type (node);

          /* Drop the color node because we get that for free from our
           * background recoloring. This avoids an extra large overdraw
           * as a bonus optimization while we fix clipping.
           */
          if (!dropped_bg && node_type == GSK_COLOR_NODE)
            {
              dropped_bg = TRUE;
              continue;
            }

          /* If we get a clip node here, it's because we're in some
           * sort of window size that has partial line offset in the
           * drag resize, or we're scrolled up a bit so the line doesn't
           * exactly match our actual sizing. In that case we'll replace
           * the clip with our own so that we get nice padding normally
           * but appropriate draws up to the border elsewise.
           */
          if (node_type == GSK_CLIP_NODE)
            node = gsk_clip_node_get_child (node);

          g_ptr_array_add (children, node);
        }
    }
  else if (gsk_render_node_get_node_type (root) == GSK_COLOR_NODE)
    {
      /* We got a single (background color) node, just ignore it */
      return;
    }

  if (children->len > 0)
    {
      GskRenderNode *new_root;

      new_root = gsk_container_node_new ((GskRenderNode **)children->pdata, children->len);
      gsk_render_node_unref (root);
      root = new_root;
    }

  gtk_snapshot_append_node (snapshot, root);
}

static void
ide_terminal_snapshot (GtkWidget   *widget,
                       GtkSnapshot *snapshot)
{
  IdeTerminal *self = IDE_TERMINAL (widget);
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (GTK_IS_SNAPSHOT (snapshot));

  ide_terminal_rewrite_snapshot (widget, snapshot);

  gtk_widget_snapshot_child (widget, GTK_WIDGET (priv->size_revealer), snapshot);
  gtk_widget_snapshot_child (widget, GTK_WIDGET (priv->drop_highlight), snapshot);
}

static void
ide_terminal_font_changed (IdeTerminal *self,
                           const char  *key,
                           GSettings   *settings_)
{
  g_autoptr(PangoFontDescription) font_desc = NULL;
  g_autofree char *font_name = NULL;
  const char *system_font_name;

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (G_IS_SETTINGS (settings_));

  font_name = g_settings_get_string (settings_, "font-name");
  system_font_name = ide_application_get_system_font_name (IDE_APPLICATION_DEFAULT);

  if (g_settings_get_boolean (settings_, "use-custom-font"))
    font_desc = pango_font_description_from_string (font_name);
  else
    font_desc = pango_font_description_from_string (system_font_name);

  vte_terminal_set_font (VTE_TERMINAL (self), font_desc);
}

static void
update_scrollback_cb (IdeTerminal *self,
                      const char  *key,
                      GSettings   *settings_)
{
  gboolean limit_scrollback;
  guint scrollback_lines;

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (G_IS_SETTINGS (settings_));

  limit_scrollback = g_settings_get_boolean (settings_, "limit-scrollback");
  scrollback_lines = g_settings_get_uint (settings_, "scrollback-lines");

  if (limit_scrollback)
    vte_terminal_set_scrollback_lines (VTE_TERMINAL (self), scrollback_lines);
  else
    vte_terminal_set_scrollback_lines (VTE_TERMINAL (self), -1);
}

static void
ide_terminal_constructed (GObject *object)
{
  IdeTerminal *self = (IdeTerminal *)object;

  g_assert (IDE_IS_TERMINAL (self));

  if (settings == NULL)
    settings = g_settings_new ("org.gnome.builder.terminal");

  G_OBJECT_CLASS (ide_terminal_parent_class)->constructed (object);

  adw_style_manager_get_dark (adw_style_manager_get_default());
  g_signal_connect_object (adw_style_manager_get_default (),
                           "notify::color-scheme",
                           G_CALLBACK (ide_terminal_update_colors),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (adw_style_manager_get_default (),
                           "notify::dark",
                           G_CALLBACK (ide_terminal_update_colors),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (IDE_APPLICATION_DEFAULT,
                           "notify::style-scheme",
                           G_CALLBACK (ide_terminal_update_colors),
                           self,
                           G_CONNECT_SWAPPED);

  ide_terminal_update_colors (self);

  g_settings_bind (settings, "allow-bold", self, "allow-bold", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "allow-hyperlink", self, "allow-hyperlink", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "scroll-on-output", self, "scroll-on-output", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "scroll-on-keystroke", self, "scroll-on-keystroke", G_SETTINGS_BIND_GET);
  g_signal_connect_object (settings,
                           "changed::limit-scrollback",
                           G_CALLBACK (update_scrollback_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (settings,
                           "changed::scrollback-lines",
                           G_CALLBACK (update_scrollback_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (settings,
                           "changed::font-name",
                           G_CALLBACK (ide_terminal_font_changed),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (settings,
                           "changed::use-custom-font",
                           G_CALLBACK (ide_terminal_font_changed),
                           self,
                           G_CONNECT_SWAPPED);
  ide_terminal_font_changed (self, NULL, settings);
  update_scrollback_cb (self, "scrollback-lines", settings);
}

static void
ide_terminal_dispose (GObject *object)
{
  IdeTerminal *self = (IdeTerminal *)object;
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);

  g_clear_pointer ((GtkWidget **)&priv->popover, gtk_widget_unparent);

  gtk_widget_dispose_template (GTK_WIDGET (self), IDE_TYPE_TERMINAL);

  g_clear_object (&priv->palette);
  g_clear_handle_id (&priv->size_dismiss_source, g_source_remove);
  g_clear_pointer (&priv->url, g_free);

  G_OBJECT_CLASS (ide_terminal_parent_class)->dispose (object);
}

static void
ide_terminal_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeTerminal *self = IDE_TERMINAL (object);

  switch (prop_id)
    {
    case PROP_PALETTE:
      g_value_set_object (value, ide_terminal_get_palette (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_terminal_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeTerminal *self = IDE_TERMINAL (object);

  switch (prop_id)
    {
    case PROP_PALETTE:
      ide_terminal_set_palette (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_terminal_class_init (IdeTerminalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  VteTerminalClass *terminal_class = VTE_TERMINAL_CLASS (klass);

  object_class->constructed = ide_terminal_constructed;
  object_class->dispose = ide_terminal_dispose;
  object_class->get_property = ide_terminal_get_property;
  object_class->set_property = ide_terminal_set_property;

  widget_class->measure = ide_terminal_measure;
  widget_class->size_allocate = ide_terminal_size_allocate;
  widget_class->snapshot = ide_terminal_snapshot;

  terminal_class->selection_changed = ide_terminal_selection_changed;

  properties[PROP_PALETTE] =
    g_param_spec_object ("palette", NULL, NULL,
                         IDE_TYPE_TERMINAL_PALETTE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[MATCH_CLICKED] =
    g_signal_new ("match-clicked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_true_handled, NULL,
                  NULL,
                  G_TYPE_BOOLEAN,
                  5,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE,
                  G_TYPE_INT,
                  GDK_TYPE_MODIFIER_TYPE,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-terminal/ui/ide-terminal.ui");

  gtk_widget_class_bind_template_child_private (widget_class, IdeTerminal, drop_highlight);
  gtk_widget_class_bind_template_child_private (widget_class, IdeTerminal, drop_target);
  gtk_widget_class_bind_template_child_private (widget_class, IdeTerminal, size_label);
  gtk_widget_class_bind_template_child_private (widget_class, IdeTerminal, size_revealer);
  gtk_widget_class_bind_template_child_private (widget_class, IdeTerminal, terminal_menu);

  gtk_widget_class_bind_template_callback (widget_class, ide_terminal_bubble_click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_terminal_capture_click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_terminal_capture_key_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, ide_terminal_drop_target_drag_enter);
  gtk_widget_class_bind_template_callback (widget_class, ide_terminal_drop_target_drag_leave);
  gtk_widget_class_bind_template_callback (widget_class, ide_terminal_drop_target_drop);

  gtk_widget_class_install_action (widget_class, "clipboard.copy", NULL, copy_clipboard_action);
  gtk_widget_class_install_action (widget_class, "clipboard.copy-link", NULL, copy_link_address_action);
  gtk_widget_class_install_action (widget_class, "clipboard.paste", NULL, paste_clipboard_action);
  gtk_widget_class_install_action (widget_class, "terminal.open-link", NULL, open_link_action);
  gtk_widget_class_install_action (widget_class, "terminal.select-all", "b", select_all_action);
  gtk_widget_class_install_action (widget_class, "terminal.reset", "b", reset_action);

  gtk_widget_class_install_property_action (widget_class, "terminal.allow-input", "input-enabled");

  for (guint i = 0; i < G_N_ELEMENTS (builtin_dingus); i++)
    {
      g_autoptr(GError) error = NULL;

      builtin_dingus_regex[i] = vte_regex_new_for_match (builtin_dingus[i],
                                                         strlen (builtin_dingus[i]),
                                                         VTE_REGEX_FLAGS_DEFAULT | VTE_PCRE2_MULTILINE | VTE_PCRE2_UCP,
                                                         NULL);

      if (!vte_regex_jit (builtin_dingus_regex[i], 0, &error))
        g_warning ("Failed to JIT regex: %s: Regex was: %s",
                   error->message,
                   builtin_dingus[i]);
    }

  filename_regex = g_regex_new (FILENAME_PLUS_LOCATION, 0, 0, NULL);
  g_assert (filename_regex != NULL);
}

static void
ide_terminal_init (IdeTerminal *self)
{
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);
  g_autoptr(GdkContentFormats) formats = NULL;
  g_autoptr(GPropertyAction) read_only = NULL;
  GdkContentFormatsBuilder *builder;

  gtk_widget_init_template (GTK_WIDGET (self));

  for (guint i = 0; i < G_N_ELEMENTS (builtin_dingus_regex); i++)
    {
      int tag = vte_terminal_match_add_regex (VTE_TERMINAL (self),
                                              builtin_dingus_regex[i],
                                              0);
      vte_terminal_match_set_cursor_name (VTE_TERMINAL (self),
                                          tag,
                                          URL_MATCH_CURSOR_NAME);
    }

  builder = gdk_content_formats_builder_new ();
  gdk_content_formats_builder_add_gtype (builder, G_TYPE_STRING);
  gdk_content_formats_builder_add_gtype (builder, GDK_TYPE_FILE_LIST);
  gdk_content_formats_builder_add_mime_type (builder, APPLICATION_VND_PORTAL_FILES);
  gdk_content_formats_builder_add_mime_type (builder, APPLICATION_VND_PORTAL_FILETRANSFER);
  gdk_content_formats_builder_add_mime_type (builder, TEXT_URI_LIST);
  gdk_content_formats_builder_add_mime_type (builder, TEXT_X_MOZ_URL);
  formats = gdk_content_formats_builder_free_to_formats (builder);

  gtk_drop_target_async_set_actions (priv->drop_target,
                                     (GDK_ACTION_COPY |
                                      GDK_ACTION_MOVE));
  gtk_drop_target_async_set_formats (priv->drop_target, formats);
}

/**
 * ide_terminal_get_palette:
 * @self: a #IdeTerminal
 *
 * Returns: (transfer none): an #IdeTerminalPalette
 */
IdeTerminalPalette *
ide_terminal_get_palette (IdeTerminal *self)
{
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TERMINAL (self), NULL);

  return priv->palette;
}

void
ide_terminal_set_palette (IdeTerminal        *self,
                          IdeTerminalPalette *palette)
{
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);

  g_return_if_fail (IDE_IS_TERMINAL (self));

  if (g_set_object (&priv->palette, palette))
    {
      ide_terminal_update_colors (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PALETTE]);
    }
}
