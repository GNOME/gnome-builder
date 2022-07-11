/* ide-terminal.c
 *
 * Copyright 2016-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-terminal"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "ide-terminal.h"
#include "ide-terminal-search.h"

#define BUILDER_PCRE2_MULTILINE 0x00000400u
#define BUILDER_PCRE2_UCP 0x00020000u

typedef struct
{
  GtkWidget *popup_menu;
  GSettings *settings;
  gchar     *url;
  GdkRGBA    bg;
  GdkRGBA    fg;
} IdeTerminalPrivate;

typedef struct
{
  IdeTerminal *terminal;
  GdkEvent   *event;
} PopupInfo;

typedef struct
{
  gint line;
  gint column;
} Position;

G_DEFINE_TYPE_WITH_PRIVATE (IdeTerminal, ide_terminal, VTE_TYPE_TERMINAL)

enum {
  COPY_LINK_ADDRESS,
  OPEN_LINK,
  POPULATE_POPUP,
  SELECT_ALL,
  SEARCH_REVEAL,
  COLORS_CHANGED,
  N_SIGNALS
};

/* From vteapp.c */
#define DINGUS1 "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?"
#define DINGUS2 DINGUS1 "/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"]"
#define FILENAME_PLUS_LOCATION "(?<filename>[[:alnum:]\\+\\-\\.\\/_]+):(?<line>\\d+):(?<column>\\d+)"

static guint signals[N_SIGNALS];
static const gchar *url_regexes[] = { DINGUS1, DINGUS2, FILENAME_PLUS_LOCATION };
static GRegex *filename_regex;
static const GdkRGBA solarized_palette[] = {
  /*
   * Solarized palette (1.0.0beta2):
   * http://ethanschoonover.com/solarized
   */
  { 0.02745,  0.211764, 0.258823, 1 },
  { 0.862745, 0.196078, 0.184313, 1 },
  { 0.521568, 0.6,      0,        1 },
  { 0.709803, 0.537254, 0,        1 },
  { 0.149019, 0.545098, 0.823529, 1 },
  { 0.82745,  0.211764, 0.509803, 1 },
  { 0.164705, 0.631372, 0.596078, 1 },
  { 0.933333, 0.909803, 0.835294, 1 },
  { 0,        0.168627, 0.211764, 1 },
  { 0.796078, 0.294117, 0.086274, 1 },
  { 0.345098, 0.431372, 0.458823, 1 },
  { 0.396078, 0.482352, 0.513725, 1 },
  { 0.513725, 0.580392, 0.588235, 1 },
  { 0.423529, 0.443137, 0.768627, 1 },
  { 0.57647,  0.631372, 0.631372, 1 },
  { 0.992156, 0.964705, 0.890196, 1 },
};

void
ide_terminal_get_colors (IdeTerminal *self,
                         GdkRGBA     *bg,
                         GdkRGBA     *fg)
{
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);

  g_return_if_fail (IDE_IS_TERMINAL (self));

  if (bg)
    *bg = priv->bg;

  if (fg)
    *fg = priv->fg;
}

static void
ide_terminal_css_changed (GtkWidget         *widget,
                          GtkCssStyleChange *change)
{
  IdeTerminal *self = (IdeTerminal *)widget;
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);
  GtkStyleContext *style_context;
  GdkRGBA fg;
  GdkRGBA bg;

  g_assert (IDE_IS_TERMINAL (widget));

  style_context = gtk_widget_get_style_context (widget);

  if (!gtk_style_context_lookup_color (style_context, "window_fg_color", &fg))
    gdk_rgba_parse (&fg, "#eeeeec");

  if (!gtk_style_context_lookup_color (style_context, "window_bg_color", &bg))
    gdk_rgba_parse (&bg, "#242424");

  vte_terminal_set_colors (VTE_TERMINAL (widget),
                           &fg, &bg,
                           solarized_palette, G_N_ELEMENTS (solarized_palette));

  priv->fg = fg;
  priv->bg = bg;

  g_signal_emit (self, signals [COLORS_CHANGED], 0);
}

#if 0
static void
popup_menu_detach (GtkWidget *attach_widget,
                   GtkMenu   *menu)
{
  IdeTerminal *self = IDE_TERMINAL (attach_widget);
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (GTK_IS_MENU (menu));
  g_assert (priv->popup_menu == NULL || GTK_IS_WIDGET (priv->popup_menu));

  g_clear_pointer (&priv->url, g_free);

  if (priv->popup_menu == GTK_WIDGET (menu))
    g_clear_pointer (&priv->popup_menu, gtk_widget_destroy);
}

static void
popup_targets_received (GtkClipboard     *clipboard,
                        GtkSelectionData *data,
                        gpointer          user_data)
{
  PopupInfo *popup_info = user_data;
  g_autoptr(IdeTerminal) self = NULL;
  g_autoptr(GdkEvent) event = NULL;
  IdeTerminalPrivate *priv;

  g_assert (popup_info != NULL);
  g_assert (IDE_IS_TERMINAL (popup_info->terminal));

  self = g_steal_pointer (&popup_info->terminal);
  priv = ide_terminal_get_instance_private (self);
  event = g_steal_pointer (&popup_info->event);

  if (gtk_widget_get_realized (GTK_WIDGET (self)))
    {
      DzlWidgetActionGroup *group;
      GMenu *menu;
      gboolean clipboard_contains_text;
      gboolean have_selection;

      clipboard_contains_text = gtk_selection_data_targets_include_text (data);
      have_selection = vte_terminal_get_has_selection (VTE_TERMINAL (self));

      g_clear_pointer (&priv->popup_menu, gtk_widget_destroy);

      priv->url = vte_terminal_match_check_event (VTE_TERMINAL (self), event, NULL);

      menu = dzl_application_get_menu_by_id (DZL_APPLICATION_DEFAULT, "ide-terminal-page-popup-menu");
      priv->popup_menu = gtk_menu_new_from_model (G_MENU_MODEL (menu));

      group = DZL_WIDGET_ACTION_GROUP (gtk_widget_get_action_group (GTK_WIDGET (self), "terminal"));

      dzl_widget_action_group_set_action_enabled (group, "copy-link-address", priv->url != NULL);
      dzl_widget_action_group_set_action_enabled (group, "open-link", priv->url != NULL);
      dzl_widget_action_group_set_action_enabled (group, "copy-clipboard", have_selection);
      dzl_widget_action_group_set_action_enabled (group, "paste-clipboard", clipboard_contains_text);

      dzl_gtk_widget_add_style_class (priv->popup_menu, GTK_STYLE_CLASS_CONTEXT_MENU);
      gtk_menu_attach_to_widget (GTK_MENU (priv->popup_menu), GTK_WIDGET (self), popup_menu_detach);

      g_signal_emit (self, signals[POPULATE_POPUP], 0, priv->popup_menu);

      gtk_menu_popup_at_pointer (GTK_MENU (priv->popup_menu), event);
    }

  g_slice_free (PopupInfo, popup_info);
}

static void
ide_terminal_do_popup (IdeTerminal    *self,
                       const GdkEvent *event)
{
  PopupInfo *popup_info;
  GtkClipboard *clipboard;

  g_assert (IDE_IS_TERMINAL (self));

  popup_info = g_slice_new0 (PopupInfo);
  popup_info->event = event ? gdk_event_copy (event) : gtk_get_current_event ();
  popup_info->terminal = g_object_ref (self);

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self), GDK_SELECTION_CLIPBOARD);

  gtk_clipboard_request_contents (clipboard,
                                  gdk_atom_intern_static_string ("TARGETS"),
                                  popup_targets_received,
                                  popup_info);
}

static gboolean
ide_terminal_popup_menu (GtkWidget *widget)
{
  IdeTerminal *self = (IdeTerminal *)widget;

  g_assert (IDE_IS_TERMINAL (self));

  ide_terminal_do_popup (self, NULL);

  return TRUE;
}
#endif

static void
ide_terminal_click_pressed_cb (IdeTerminal     *self,
                               int              n_presses,
                               double           x,
                               double           y,
                               GtkGestureClick *click)
{
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);
  int button;

  IDE_ENTRY;

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (click));

  if (button == 1)
    {
      g_autofree gchar *pattern = NULL;
      glong cell_width = vte_terminal_get_char_width (VTE_TERMINAL (self));
      glong cell_height = vte_terminal_get_char_height (VTE_TERMINAL (self));
      glong column, row;
      int tag = 0;

      /* crappy way to do this, but i dont see another option right
       * now given we have to go through deprecated APIs in Vte
       * until it gets things together for GTK 4.
       */
      column = x / cell_width;
      row = y / cell_height;

      /* no other option in VTE for GTK 4 right now */
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      pattern = vte_terminal_match_check (VTE_TERMINAL (self), column, row, &tag);
      G_GNUC_END_IGNORE_DEPRECATIONS

      if (pattern != NULL)
        {
          gboolean ret = GDK_EVENT_PROPAGATE;

          ide_set_string (&priv->url, pattern);

          g_signal_emit (self, signals[OPEN_LINK], 0, &ret);

          if (ret)
            gtk_gesture_set_state (GTK_GESTURE (click), GTK_EVENT_SEQUENCE_CLAIMED);
        }
    }
#if 0
  else if (button == 3)
    {
      if (!gtk_widget_has_focus (GTK_WIDGET (self)))
        gtk_widget_grab_focus (GTK_WIDGET (self));

      ide_terminal_do_popup (self, (GdkEvent *)button);
    }
#endif

  IDE_EXIT;
}

static void
ide_terminal_real_select_all (IdeTerminal *self,
                              gboolean     all)
{
  g_assert (IDE_IS_TERMINAL (self));

  if (all)
    vte_terminal_select_all (VTE_TERMINAL (self));
  else
    vte_terminal_unselect_all (VTE_TERMINAL (self));
}

static gboolean
ide_terminal_copy_link_address (IdeTerminal *self)
{
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (priv->url != NULL);

  if (ide_str_empty0 (priv->url))
    return FALSE;

  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (self)), priv->url);

  return TRUE;
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

static gboolean
ide_terminal_open_link (IdeTerminal *self)
{
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);
  g_autoptr(GMatchInfo) match = NULL;
  g_autofree gchar *filename = NULL;
  g_autofree gchar *line = NULL;
  g_autofree gchar *column = NULL;
  GtkApplication *app;
  GtkWindow *focused_window;

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (priv->url != NULL);

  if (ide_str_empty0 (priv->url))
    return FALSE;

  if (g_regex_match (filename_regex, priv->url, 0, &match) &&
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

      return TRUE;
    }

  if ((app = GTK_APPLICATION (g_application_get_default ())) &&
      (focused_window = gtk_application_get_active_window (app)))
    return ide_gtk_show_uri_on_window (focused_window,
                                       priv->url,
                                       g_get_monotonic_time (),
                                       NULL);

  return FALSE;
}

static GtkWidget *
find_child_typed (GtkWidget *parent,
                  GType      child_type)
{
  for (GtkWidget *child = gtk_widget_get_first_child (parent);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      if (g_type_is_a (G_OBJECT_TYPE (child), child_type))
        return child;
    }

  return NULL;
}

static void
ide_terminal_search_reveal (GtkWidget  *widget,
                            const char *action_name,
                            GVariant   *param)
{
  IdeTerminal *self = (IdeTerminal *)widget;
  GtkWidget *parent_overlay;

  g_assert (IDE_IS_TERMINAL (self));

  parent_overlay = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_OVERLAY);

  if (parent_overlay != NULL)
    {
      IdeTerminalSearch *search = IDE_TERMINAL_SEARCH (find_child_typed (parent_overlay, IDE_TYPE_TERMINAL_SEARCH));

      if (search != NULL)
        {
          GtkRevealer *revealer = ide_terminal_search_get_revealer (search);

          if (!gtk_revealer_get_child_revealed (revealer))
            gtk_revealer_set_reveal_child (revealer, TRUE);
        }
    }
}

static void
ide_terminal_font_changed (IdeTerminal *self,
                           const gchar *key,
                           GSettings   *settings)
{
  PangoFontDescription *font_desc = NULL;
  g_autofree gchar *font_name = NULL;

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (G_IS_SETTINGS (settings));

  font_name = g_settings_get_string (settings, "font-name");

  if (font_name != NULL)
    font_desc = pango_font_description_from_string (font_name);

  vte_terminal_set_font (VTE_TERMINAL (self), font_desc);
  g_clear_pointer (&font_desc, pango_font_description_free);
}

static void
ide_terminal_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  IdeTerminal *self = (IdeTerminal *)widget;
  int char_width, char_height;
  int columns, rows;

  GTK_WIDGET_CLASS (ide_terminal_parent_class)->size_allocate (widget, width, height, baseline);

  if (width == 0 || height == 0)
    return;

  char_width = vte_terminal_get_char_width (VTE_TERMINAL (self));
  char_height = vte_terminal_get_char_height (VTE_TERMINAL (self));
  if (char_width == 0 || char_height == 0)
    return;

  columns = width / char_width;
  rows = height / char_height;
  if (columns < 2 || rows < 2)
    return;

  vte_terminal_set_size (VTE_TERMINAL (self), columns, rows);
}

static void
update_scrollback_cb (IdeTerminal *self,
                      const char  *key,
                      GSettings   *settings)
{
  gboolean limit_scrollback;
  guint scrollback_lines;

  g_assert (IDE_IS_TERMINAL (self));
  g_assert (G_IS_SETTINGS (settings));

  limit_scrollback = g_settings_get_boolean (settings, "limit-scrollback");
  scrollback_lines = g_settings_get_uint (settings, "scrollback-lines");

  if (limit_scrollback)
    vte_terminal_set_scrollback_lines (VTE_TERMINAL (self), scrollback_lines);
  else
    vte_terminal_set_scrollback_lines (VTE_TERMINAL (self), -1);
}

static void
copy_clipboard_action (GtkWidget  *widget,
                       const char *action_name,
                       GVariant   *param)
{
  g_signal_emit_by_name (widget, "copy-clipboard");
}

static void
paste_clipboard_action (GtkWidget  *widget,
                        const char *action_name,
                        GVariant   *param)
{
  g_signal_emit_by_name (widget, "paste-clipboard");
}

static void
ide_terminal_dispose (GObject *object)
{
  IdeTerminal *self = (IdeTerminal *)object;
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);

  g_clear_object (&priv->settings);
  g_clear_pointer (&priv->url, g_free);

  G_OBJECT_CLASS (ide_terminal_parent_class)->dispose (object);
}

static void
ide_terminal_class_init (IdeTerminalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_terminal_dispose;

  widget_class->css_changed = ide_terminal_css_changed;
  widget_class->size_allocate = ide_terminal_size_allocate;

  klass->copy_link_address = ide_terminal_copy_link_address;
  klass->open_link = ide_terminal_open_link;
  klass->select_all = ide_terminal_real_select_all;

  filename_regex = g_regex_new (FILENAME_PLUS_LOCATION, 0, 0, NULL);
  g_assert (filename_regex != NULL);

  signals [COLORS_CHANGED] =
    g_signal_new ("colors-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals [COPY_LINK_ADDRESS] =
    g_signal_new ("copy-link-address",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeTerminalClass, copy_link_address),
                  NULL, NULL, NULL,
                  G_TYPE_BOOLEAN,
                  0);

  signals [OPEN_LINK] =
    g_signal_new ("open-link",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeTerminalClass, open_link),
                  g_signal_accumulator_true_handled, NULL,
                  NULL,
                  G_TYPE_BOOLEAN, 0);

  signals [POPULATE_POPUP] =
    g_signal_new ("populate-popup",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeTerminalClass, populate_popup),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_WIDGET);

  signals [SELECT_ALL] =
    g_signal_new ("select-all",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeTerminalClass, select_all),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  gtk_widget_class_install_action (widget_class, "terminal.copy-clipboard", NULL, copy_clipboard_action);
  gtk_widget_class_install_action (widget_class, "terminal.paste-clipboard", NULL, paste_clipboard_action);
  gtk_widget_class_install_action (widget_class, "terminal.search-reveal", NULL, ide_terminal_search_reveal);
}

static void
ide_terminal_init (IdeTerminal *self)
{
  IdeTerminalPrivate *priv = ide_terminal_get_instance_private (self);
  GtkEventController *gesture;

  gesture = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  g_signal_connect_object (gesture,
                           "pressed",
                           G_CALLBACK (ide_terminal_click_pressed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_add_controller (GTK_WIDGET (self), g_steal_pointer (&gesture));

  for (guint i = 0; i < G_N_ELEMENTS (url_regexes); i++)
    {
      g_autoptr(VteRegex) regex = NULL;
      const gchar *pattern = url_regexes[i];
      gint tag;

      regex = vte_regex_new_for_match (pattern, strlen (pattern),
                                       VTE_REGEX_FLAGS_DEFAULT | BUILDER_PCRE2_MULTILINE | BUILDER_PCRE2_UCP,
                                       NULL);
      tag = vte_terminal_match_add_regex (VTE_TERMINAL (self), regex, 0);
      vte_terminal_match_set_cursor_name (VTE_TERMINAL (self), tag, "hand2");
    }

  priv->settings = g_settings_new ("org.gnome.builder.terminal");
  g_settings_bind (priv->settings, "allow-bold", self, "allow-bold", G_SETTINGS_BIND_GET);
  g_settings_bind (priv->settings, "allow-hyperlink", self, "allow-hyperlink", G_SETTINGS_BIND_GET);
  g_settings_bind (priv->settings, "scroll-on-output", self, "scroll-on-output", G_SETTINGS_BIND_GET);
  g_settings_bind (priv->settings, "scroll-on-keystroke", self, "scroll-on-keystroke", G_SETTINGS_BIND_GET);
  g_signal_connect_object (priv->settings,
                           "changed::limit-scrollback",
                           G_CALLBACK (update_scrollback_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->settings,
                           "changed::scrollback-lines",
                           G_CALLBACK (update_scrollback_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->settings,
                           "changed::font-name",
                           G_CALLBACK (ide_terminal_font_changed),
                           self,
                           G_CONNECT_SWAPPED);
  ide_terminal_font_changed (self, NULL, priv->settings);
  update_scrollback_cb (self, "scrollback-lines", priv->settings);

  vte_terminal_set_enable_fallback_scrolling (VTE_TERMINAL (self), FALSE);
  vte_terminal_set_scroll_unit_is_pixels (VTE_TERMINAL (self), TRUE);
}

GtkWidget *
ide_terminal_new (void)
{
  return g_object_new (IDE_TYPE_TERMINAL, NULL);
}
