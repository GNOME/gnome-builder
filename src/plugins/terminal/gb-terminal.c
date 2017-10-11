/* gb-terminal.c
 *
 * Copyright © 2016 Christian Hergert <christian@hergert.me>
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

#include <dazzle.h>
#include <glib/gi18n.h>
#include <ide.h>

#include "gb-terminal.h"

#define BUILDER_PCRE2_MULTILINE           0x00000400u

typedef struct
{
  GbTerminal *terminal;
  GdkEvent   *event;
} PopupInfo;

struct _GbTerminal
{
  VteTerminal  parent;

  GtkWidget   *popup_menu;

  gchar       *url;
};

struct _GbTerminalClass
{
  VteTerminalClass parent;

  void     (*populate_popup)      (GbTerminal *self,
                                   GtkWidget  *widget);
  void     (*select_all)          (GbTerminal *self,
                                   gboolean    all);
  void     (*search_reveal)       (GbTerminal *self);
  gboolean (*open_link)           (GbTerminal *self);
  gboolean (*copy_link_address)   (GbTerminal *self);
};

G_DEFINE_TYPE (GbTerminal, gb_terminal, VTE_TYPE_TERMINAL)

enum {
  COPY_LINK_ADDRESS,
  OPEN_LINK,
  POPULATE_POPUP,
  SELECT_ALL,
  SEARCH_REVEAL,
  LAST_SIGNAL
};

/* From vteapp.c */
#define DINGUS1 "(((gopher|news|telnet|nntp|file|http|ftp|https)://)|(www|ftp)[-A-Za-z0-9]*\\.)[-A-Za-z0-9\\.]+(:[0-9]*)?"
#define DINGUS2 DINGUS1 "/[-A-Za-z0-9_\\$\\.\\+\\!\\*\\(\\),;:@&=\\?/~\\#\\%]*[^]'\\.}>\\) ,\\\"]"

static guint signals [LAST_SIGNAL];
static const gchar *url_regexes[] = {
  DINGUS1,
  DINGUS2,
  NULL
};

/* The popup code is an adaptation of GtktextView popupmenu functions */
static void
popup_menu_detach (GtkWidget *attach_widget,
                   GtkMenu   *menu)
{
  GbTerminal *terminal = GB_TERMINAL (attach_widget);

  terminal->popup_menu = NULL;

  g_clear_pointer (&terminal->url, g_free);
}

static void
popup_targets_received (GtkClipboard     *clipboard,
                        GtkSelectionData *data,
                        gpointer          user_data)
{
  GMenu *menu;
  PopupInfo *popup_info = user_data;
  GbTerminal *terminal = popup_info->terminal;

  if (gtk_widget_get_realized (GTK_WIDGET (terminal)))
    {
      GActionGroup *group;
      gboolean clipboard_contains_text;
      gboolean have_selection;

      clipboard_contains_text = gtk_selection_data_targets_include_text (data);
      have_selection = vte_terminal_get_has_selection (VTE_TERMINAL (terminal));

      if (terminal->popup_menu)
        gtk_widget_destroy (terminal->popup_menu);

      terminal->url  = vte_terminal_match_check_event (VTE_TERMINAL (terminal), (GdkEvent *)popup_info->event, NULL);

      menu = dzl_application_get_menu_by_id (DZL_APPLICATION_DEFAULT, "gb-terminal-view-popup-menu");
      terminal->popup_menu = gtk_menu_new_from_model (G_MENU_MODEL (menu));

      group = gtk_widget_get_action_group (GTK_WIDGET (terminal), "terminal");

      dzl_widget_action_group_set_action_enabled (DZL_WIDGET_ACTION_GROUP (group), "copy-link-address", !(terminal->url == NULL));
      dzl_widget_action_group_set_action_enabled (DZL_WIDGET_ACTION_GROUP (group), "open-link", !(terminal->url == NULL));
      dzl_widget_action_group_set_action_enabled (DZL_WIDGET_ACTION_GROUP (group), "copy-clipboard", have_selection);
      dzl_widget_action_group_set_action_enabled (DZL_WIDGET_ACTION_GROUP (group), "paste-clipboard", clipboard_contains_text);

      gtk_style_context_add_class (gtk_widget_get_style_context (terminal->popup_menu), GTK_STYLE_CLASS_CONTEXT_MENU);
      gtk_menu_attach_to_widget (GTK_MENU (terminal->popup_menu), GTK_WIDGET (terminal), popup_menu_detach);

      g_signal_emit (terminal, signals[POPULATE_POPUP], 0, terminal->popup_menu);

      gtk_menu_popup_at_pointer (GTK_MENU (terminal->popup_menu), popup_info->event);
      gdk_event_free (popup_info->event);
    }

  g_object_unref (terminal);
  g_slice_free (PopupInfo, popup_info);
}

static void
gb_terminal_do_popup (GbTerminal     *terminal,
                      const GdkEvent *event)
{
  PopupInfo *popup_info = g_slice_new (PopupInfo);

  popup_info->event = (event != NULL) ? gdk_event_copy (event)
                                      : gtk_get_current_event ();

  popup_info->terminal = g_object_ref (terminal);

  gtk_clipboard_request_contents (gtk_widget_get_clipboard (GTK_WIDGET (terminal), GDK_SELECTION_CLIPBOARD),
                                  gdk_atom_intern_static_string ("TARGETS"),
                                  popup_targets_received,
                                  popup_info);
}

static gboolean
gb_terminal_popup_menu (GtkWidget *widget)
{
  gb_terminal_do_popup (GB_TERMINAL (widget), NULL);
  return TRUE;
}

static gboolean
gb_terminal_button_press_event (GtkWidget      *widget,
                                GdkEventButton *button)
{
  GbTerminal *self = (GbTerminal *)widget;

  g_assert (GB_IS_TERMINAL (self));
  g_assert (button != NULL);

  if ((button->type == GDK_BUTTON_PRESS) && (button->button == GDK_BUTTON_SECONDARY))
    {
      if (!gtk_widget_has_focus (GTK_WIDGET (self)))
        gtk_widget_grab_focus (GTK_WIDGET (self));

      gb_terminal_do_popup (self, (GdkEvent *)button);
      return GDK_EVENT_STOP;
    }
  else if ((button->type == GDK_BUTTON_PRESS) && (button->button == GDK_BUTTON_PRIMARY)
            && ((button->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK))
    {
      g_autofree gchar *pattern = NULL;

      pattern = vte_terminal_match_check_event (VTE_TERMINAL (self), (GdkEvent *)button, NULL);

      if (pattern != NULL)
        {
          GtkApplication *app;
          GtkWindow *focused_window;

          if (NULL != (app = GTK_APPLICATION (g_application_get_default ())) &&
              NULL != (focused_window = gtk_application_get_active_window (app)))
            {
              gtk_show_uri_on_window (focused_window,
                                      pattern,
                                      gtk_get_current_event_time (),
                                      NULL);
            }
        }

      return GDK_EVENT_STOP;
    }

  return GTK_WIDGET_CLASS (gb_terminal_parent_class)->button_press_event (widget, button);
}

static void
gb_terminal_real_select_all (GbTerminal *self,
                             gboolean    all)
{
  g_assert (GB_IS_TERMINAL (self));

  if (all)
    vte_terminal_select_all (VTE_TERMINAL (self));
  else
    vte_terminal_unselect_all (VTE_TERMINAL (self));
}

static gboolean
gb_terminal_copy_link_address (GbTerminal *self)
{
  g_assert (GB_IS_TERMINAL (self));
  g_assert (self->url != NULL);

  if (ide_str_empty0 (self->url))
    return FALSE;

  gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (self), GDK_SELECTION_CLIPBOARD),
                          self->url,
                          strlen (self->url));

  return TRUE;
}

static gboolean
gb_terminal_open_link (GbTerminal *self)
{
  GtkApplication *app;
  GtkWindow *focused_window;

  g_assert (GB_IS_TERMINAL (self));
  g_assert (self->url != NULL);

  if (ide_str_empty0 (self->url))
    return FALSE;

  if (NULL != (app = GTK_APPLICATION (g_application_get_default ())) &&
      NULL != (focused_window = gtk_application_get_active_window (app)))
    {
      return gtk_show_uri_on_window (focused_window,
                                     self->url,
                                     gtk_get_current_event_time (),
                                     NULL);
    }
  else
    return FALSE;
}

static void
gb_terminal_real_search_reveal (GbTerminal *self)
{
  GtkWidget *parent_overlay;

  g_assert (GB_IS_TERMINAL (self));

  parent_overlay = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_OVERLAY);

  if (parent_overlay != NULL)
    {
      GtkRevealer *revealer = dzl_gtk_widget_find_child_typed (parent_overlay, GTK_TYPE_REVEALER);

      if (revealer != NULL && !gtk_revealer_get_child_revealed (revealer))
        gtk_revealer_set_reveal_child (revealer, TRUE);
    }
}

static void
gb_terminal_class_init (GbTerminalClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkBindingSet *binding_set;

  widget_class->button_press_event = gb_terminal_button_press_event;
  widget_class->popup_menu = gb_terminal_popup_menu;

  klass->copy_link_address = gb_terminal_copy_link_address;
  klass->open_link = gb_terminal_open_link;
  klass->select_all = gb_terminal_real_select_all;
  klass->search_reveal = gb_terminal_real_search_reveal;

  signals [COPY_LINK_ADDRESS] =
    g_signal_new ("copy-link-address",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GbTerminalClass, copy_link_address),
                  NULL, NULL, NULL,
                  G_TYPE_BOOLEAN,
                  0);

  signals [SEARCH_REVEAL] =
    g_signal_new ("search-reveal",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GbTerminalClass, search_reveal),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals [OPEN_LINK] =
    g_signal_new ("open-link",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GbTerminalClass, open_link),
                  NULL, NULL, NULL,
                  G_TYPE_BOOLEAN,
                  0);

  signals [POPULATE_POPUP] =
    g_signal_new ("populate-popup",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbTerminalClass, populate_popup),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_WIDGET);

  signals [SELECT_ALL] =
    g_signal_new ("select-all",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GbTerminalClass, select_all),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_c,
                                GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                "copy-clipboard",
                                0);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_v,
                                GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                "paste-clipboard",
                                0);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_f,
                                GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                "search-reveal",
                                0);
}

static void
gb_terminal_init (GbTerminal *self)
{
  dzl_widget_action_group_attach (self, "terminal");

  for (guint i = 0; url_regexes[i]; i++)
    {
      const gchar *pattern = url_regexes[i];
      g_autoptr(VteRegex) regex = NULL;
      gint tag;

      regex = vte_regex_new_for_match (pattern, IDE_LITERAL_LENGTH (pattern),
                                       VTE_REGEX_FLAGS_DEFAULT | BUILDER_PCRE2_MULTILINE,
                                       NULL);
      tag = vte_terminal_match_add_regex (VTE_TERMINAL (self), regex, 0);
      vte_terminal_match_set_cursor_type (VTE_TERMINAL (self), tag, GDK_HAND2);
    }
}
