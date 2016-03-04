/* gb-terminal.c
 *
 * Copyright (C) 2016 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>
#include <ide.h>

#include "egg-widget-action-group.h"

#include "gb-terminal.h"

typedef struct
{
  GbTerminal *terminal;
  guint       button;
  guint       time;
  GdkDevice  *device;
} PopupInfo;

struct _GbTerminal
{
  VteTerminal  parent;

  GtkWidget   *popup_menu;
};

struct _GbTerminalClass
{
  VteTerminalClass parent;

  void (*populate_popup) (GbTerminal *self,
                          GtkWidget  *widget);
  void (*select_all)     (GbTerminal *self,
                          gboolean    all);
};

G_DEFINE_TYPE (GbTerminal, gb_terminal, VTE_TYPE_TERMINAL)

enum {
  POPULATE_POPUP,
  SELECT_ALL,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

/* The popup code is an adaptation of GtktextView popupmenu functions */
static void
popup_menu_detach (GtkWidget *attach_widget,
                   GtkMenu   *menu)
{
  GbTerminal *terminal = GB_TERMINAL (attach_widget);

  terminal->popup_menu = NULL;
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
      GAction *action;
      gboolean clipboard_contains_text;
      gboolean have_selection;

      clipboard_contains_text = gtk_selection_data_targets_include_text (data);
      have_selection = vte_terminal_get_has_selection (VTE_TERMINAL (terminal));

      if (terminal->popup_menu)
        gtk_widget_destroy (terminal->popup_menu);

      menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "gb-terminal-view-popup-menu");
      terminal->popup_menu = gtk_menu_new_from_model (G_MENU_MODEL (menu));

      group = gtk_widget_get_action_group (GTK_WIDGET (terminal), "terminal");

      action = g_action_map_lookup_action (G_ACTION_MAP (group), "copy-clipboard");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), have_selection);
      action = g_action_map_lookup_action (G_ACTION_MAP (group), "paste-clipboard");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), clipboard_contains_text);

      gtk_style_context_add_class (gtk_widget_get_style_context (terminal->popup_menu), GTK_STYLE_CLASS_CONTEXT_MENU);
      gtk_menu_attach_to_widget (GTK_MENU (terminal->popup_menu), GTK_WIDGET (terminal), popup_menu_detach);

      g_signal_emit (terminal, signals[POPULATE_POPUP], 0, terminal->popup_menu);

      if (popup_info->device)
        gtk_menu_popup_for_device (GTK_MENU (terminal->popup_menu),
                                   popup_info->device, NULL, NULL, NULL, NULL, NULL,
                                   popup_info->button, popup_info->time);
      else
        {
          gtk_menu_popup (GTK_MENU (terminal->popup_menu), NULL, NULL,
                          NULL, terminal,
                          0, gtk_get_current_event_time ());

          gtk_menu_shell_select_first (GTK_MENU_SHELL (terminal->popup_menu), FALSE);
        }
    }

  g_object_unref (terminal);
  g_slice_free (PopupInfo, popup_info);
}

static void
gb_terminal_do_popup (GbTerminal     *terminal,
                      const GdkEvent *event)
{
  PopupInfo *popup_info = g_slice_new (PopupInfo);

  popup_info->terminal = g_object_ref (terminal);

  if (event != NULL)
    {
      gdk_event_get_button (event, &popup_info->button);
      popup_info->time = gdk_event_get_time (event);
      popup_info->device = gdk_event_get_device (event);
    }
  else
    {
      popup_info->button = 0;
      popup_info->time = gtk_get_current_event_time ();
      popup_info->device = NULL;
    }

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

static void
gb_terminal_class_init (GbTerminalClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkBindingSet *binding_set;

  widget_class->button_press_event = gb_terminal_button_press_event;
  widget_class->popup_menu = gb_terminal_popup_menu;

  klass->select_all = gb_terminal_real_select_all;

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
}

static void
gb_terminal_init (GbTerminal *self)
{
  egg_widget_action_group_attach (self, "terminal");
}
