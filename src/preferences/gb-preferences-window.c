/* gb-preferences-window.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include "gb-gdk.h"
#include "gb-preferences-page-editor.h"
#include "gb-preferences-page-experimental.h"
#include "gb-preferences-page-git.h"
#include "gb-preferences-page-keybindings.h"
#include "gb-preferences-page-language.h"
#include "gb-preferences-page.h"
#include "gb-preferences-window.h"
#include "gb-widget.h"

struct _GbPreferencesWindow
{
  GtkWindow        parent_instance;

  GtkWidget       *return_to_page;
  GtkHeaderBar    *right_header_bar;
  GtkSearchEntry  *search_entry;
  GtkSearchBar    *search_bar;
  GtkStack        *stack;
};

G_DEFINE_TYPE (GbPreferencesWindow, gb_preferences_window, GTK_TYPE_WINDOW)

enum {
  CLOSE,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

GtkWidget *
gb_preferences_window_new (void)
{
  return g_object_new (GB_TYPE_PREFERENCES_WINDOW, NULL);
}

static void
gb_preferences_window_notify_search_mode (GbPreferencesWindow *self,
                                          GParamSpec          *pspec,
                                          GtkSearchBar        *search_bar)
{
  g_return_if_fail (GB_IS_PREFERENCES_WINDOW (self));

  if (!gtk_search_bar_get_search_mode (search_bar) && self->return_to_page)
    {
      gtk_stack_set_visible_child (self->stack, self->return_to_page);
      self->return_to_page = NULL;
    }
}

static void
gb_preferences_window_section_changed (GtkStack            *stack,
                                       GParamSpec          *pspec,
                                       GbPreferencesWindow *self)
{
  GtkWidget *visible_child;
  gchar *title = NULL;

  g_return_if_fail (GTK_IS_STACK (stack));
  g_return_if_fail (GB_IS_PREFERENCES_WINDOW (self));

  visible_child = gtk_stack_get_visible_child (stack);
  if (visible_child)
    gtk_container_child_get (GTK_CONTAINER (stack), visible_child,
                             "title", &title,
                             NULL);

  gtk_header_bar_set_title (self->right_header_bar, title);

  g_free (title);
}

static void
gb_preferences_window_close (GbPreferencesWindow *self)
{
  g_assert (GB_IS_PREFERENCES_WINDOW (self));

  gtk_window_close (GTK_WINDOW (self));
}

static void
gb_preferences_window_search_changed (GbPreferencesWindow *self,
                                      GtkSearchEntry      *entry)
{
  GList *pages;
  GList *iter;
  const gchar *text;
  gchar **keywords;

  g_return_if_fail (GB_IS_PREFERENCES_WINDOW (self));
  g_return_if_fail (GTK_IS_ENTRY (entry));

  text = gtk_entry_get_text (GTK_ENTRY (entry));
  keywords = g_strsplit (text, " ", -1);

  if (g_strv_length (keywords) == 0)
    g_clear_pointer (&keywords, g_strfreev);

  pages = gtk_container_get_children (GTK_CONTAINER (self->stack));

  for (iter = pages; iter; iter = iter->next)
    {
      GbPreferencesPage *page = GB_PREFERENCES_PAGE (iter->data);

      if (0 == gb_preferences_page_set_keywords (page, (const gchar * const *)keywords))
        gtk_widget_set_visible (GTK_WIDGET (page), FALSE);
      else
        gtk_widget_set_visible (GTK_WIDGET (page), TRUE);
    }

  g_list_free (pages);
  g_strfreev (keywords);
}

static gboolean
gb_preferences_window_key_press_event (GtkWidget   *widget,
                                       GdkEventKey *event)
{
  GbPreferencesWindow *self = (GbPreferencesWindow *)widget;
  gboolean ret;
  gboolean editable = FALSE;

  g_return_val_if_fail (GB_IS_PREFERENCES_WINDOW (self), FALSE);

  /*
   * Try to propagate the event to any widget that wants to swallow it.
   */
  ret = GTK_WIDGET_CLASS (gb_preferences_window_parent_class)->key_press_event (widget, event);

  /*
   * Check if the focus widget is a GtkEditable.
   */
  editable = GTK_IS_EDITABLE (gtk_window_get_focus (GTK_WINDOW (widget)));

  if (!ret && !editable)
    {
      if (!gtk_search_bar_get_search_mode (self->search_bar))
        {
          if (gb_gdk_event_key_is_escape (event))
            {
              g_signal_emit_by_name (widget, "close");
            }
          else if (!gb_gdk_event_key_is_keynav (event) &&
                   !gb_gdk_event_key_is_space (event) &&
                   !gb_gdk_event_key_is_tab (event))
            {
              GtkWidget *current_page;

              current_page = gtk_stack_get_visible_child (self->stack);

              if (gtk_search_bar_handle_event (GTK_SEARCH_BAR (self->search_bar),
                                               (GdkEvent*) event) == GDK_EVENT_STOP)
                {
                  self->return_to_page = current_page;
                  ret = TRUE;
                }
              else
                ret = FALSE;
            }
        }
      else
        {
          if (!gtk_widget_is_focus (GTK_WIDGET (self->search_bar)) &&
              gb_gdk_event_key_is_escape (event))
            gtk_search_bar_set_search_mode (self->search_bar, FALSE);

          ret = TRUE;
        }
    }

  return ret;
}

static void
gb_preferences_window_constructed (GObject *object)
{
  GbPreferencesWindow *self = (GbPreferencesWindow *)object;

  G_OBJECT_CLASS (gb_preferences_window_parent_class)->constructed (object);

  gtk_search_bar_connect_entry (self->search_bar,
                                GTK_ENTRY (self->search_entry));

  g_signal_connect (self->stack,
                    "notify::visible-child",
                    G_CALLBACK (gb_preferences_window_section_changed),
                    self);
  gb_preferences_window_section_changed (self->stack, NULL, self);

  g_signal_connect_object (self->search_bar,
                           "notify::search-mode-enabled",
                           G_CALLBACK (gb_preferences_window_notify_search_mode),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  g_signal_connect_object (self->search_entry,
                           "changed",
                           G_CALLBACK (gb_preferences_window_search_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gb_preferences_window_class_init (GbPreferencesWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_preferences_window_constructed;

  widget_class->key_press_event = gb_preferences_window_key_press_event;

  gSignals [CLOSE] =
    g_signal_new_class_handler ("close",
                                G_TYPE_FROM_CLASS (klass),
                                (G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
                                G_CALLBACK (gb_preferences_window_close),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE,
                                0);

  GB_WIDGET_CLASS_TEMPLATE (widget_class, "gb-preferences-window.ui");

  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesWindow, right_header_bar);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesWindow, search_bar);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesWindow, search_entry);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesWindow, stack);

  g_type_ensure (GB_TYPE_PREFERENCES_PAGE_EDITOR);
  g_type_ensure (GB_TYPE_PREFERENCES_PAGE_EXPERIMENTAL);
  g_type_ensure (GB_TYPE_PREFERENCES_PAGE_GIT);
  g_type_ensure (GB_TYPE_PREFERENCES_PAGE_KEYBINDINGS);
  g_type_ensure (GB_TYPE_PREFERENCES_PAGE_LANGUAGE);
}

static void
gb_preferences_window_init (GbPreferencesWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
