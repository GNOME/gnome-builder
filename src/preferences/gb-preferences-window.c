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

#include "egg-search-bar.h"

#include "gb-gdk.h"
#include "gb-preferences-page-editor.h"
#include "gb-preferences-page-git.h"
#include "gb-preferences-page-insight.h"
#include "gb-preferences-page-keybindings.h"
#include "gb-preferences-page-language.h"
#include "gb-preferences-page-theme.h"
#include "gb-preferences-page.h"
#include "gb-preferences-window.h"
#include "gb-widget.h"

struct _GbPreferencesWindow
{
  GtkWindow        parent_instance;

  GtkWidget       *return_to_page;
  GtkHeaderBar    *right_header_bar;
  GtkSearchEntry  *search_entry;
  EggSearchBar    *search_bar;
  GtkStack        *stack;
  GtkStack        *controls_stack;
  GtkWidget       *visible_child;
  GBinding        *title_binding;

  guint            destroyed : 1;
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
gb_preferences_window_section_changed (GtkStack            *stack,
                                       GParamSpec          *pspec,
                                       GbPreferencesWindow *self)
{
  GtkWidget *visible_child;

  g_return_if_fail (GTK_IS_STACK (stack));
  g_return_if_fail (GB_IS_PREFERENCES_WINDOW (self));

  if (self->destroyed)
    return;

  visible_child = gtk_stack_get_visible_child (stack);
  if (self->visible_child != visible_child)
    {
      if (self->visible_child)
        {
          if (self->title_binding)
            g_binding_unbind (self->title_binding);
          ide_clear_weak_pointer (&self->title_binding);
          gtk_header_bar_set_title (self->right_header_bar, NULL);
          ide_clear_weak_pointer (&self->visible_child);
          gtk_widget_hide (GTK_WIDGET (self->controls_stack));
        }
      if (visible_child)
        {
          GtkWidget *controls;
          GBinding *binding;

          ide_set_weak_pointer (&self->visible_child, visible_child);
          binding = g_object_bind_property (visible_child, "title",
                                            self->right_header_bar, "title",
                                            G_BINDING_SYNC_CREATE);
          ide_set_weak_pointer (&self->title_binding, binding);

          controls = gb_preferences_page_get_controls (GB_PREFERENCES_PAGE (visible_child));
          if (controls)
            {
              gtk_stack_set_visible_child (self->controls_stack, controls);
              gtk_widget_show (GTK_WIDGET (self->controls_stack));
            }
        }
    }
}

static void
gb_preferences_window_close (GbPreferencesWindow *self)
{
  g_assert (GB_IS_PREFERENCES_WINDOW (self));

  gtk_window_close (GTK_WINDOW (self));
}

static void
gb_preferences_window_search_bar_enable_changed (GbPreferencesWindow *self,
                                                 GParamSpec          *pspec,
                                                 EggSearchBar        *search_bar)
{
  g_return_if_fail (GB_IS_PREFERENCES_WINDOW (self));
  g_return_if_fail (EGG_IS_SEARCH_BAR (search_bar));

  if(egg_search_bar_get_search_mode_enabled (search_bar))
    {
      GList *pages;
      GList *iter;

      pages = gtk_container_get_children (GTK_CONTAINER (self->stack));

      for (iter = pages; iter; iter = iter->next)
        gb_preferences_page_clear_search (GB_PREFERENCES_PAGE (iter->data));

      g_list_free (pages);
    }
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
  gboolean ret;

  g_assert (GB_IS_PREFERENCES_WINDOW (widget));

  /*
   * Try to propagate the event to any widget that wants to swallow it.
   */
  ret = GTK_WIDGET_CLASS (gb_preferences_window_parent_class)->key_press_event (widget, event);

  return ret;
}

static void
gb_preferences_window_finalize (GObject *object)
{
  GbPreferencesWindow *self = (GbPreferencesWindow *)object;

  ide_clear_weak_pointer (&self->title_binding);
  ide_clear_weak_pointer (&self->visible_child);

  G_OBJECT_CLASS (gb_preferences_window_parent_class)->finalize (object);
}

static void
gb_preferences_window_constructed (GObject *object)
{
  GbPreferencesWindow *self = (GbPreferencesWindow *)object;

  G_OBJECT_CLASS (gb_preferences_window_parent_class)->constructed (object);

  g_signal_connect (self->stack,
                    "notify::visible-child",
                    G_CALLBACK (gb_preferences_window_section_changed),
                    self);
  gb_preferences_window_section_changed (self->stack, NULL, self);

  g_signal_connect_object (self->search_entry,
                           "changed",
                           G_CALLBACK (gb_preferences_window_search_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->search_bar,
                           "notify::search-mode-enabled",
                           G_CALLBACK (gb_preferences_window_search_bar_enable_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gb_preferences_window_destroy (GtkWidget *widget)
{
  GbPreferencesWindow *self = (GbPreferencesWindow *)widget;

  g_return_if_fail (GB_IS_PREFERENCES_WINDOW (self));

  self->destroyed = TRUE;

  GTK_WIDGET_CLASS (gb_preferences_window_parent_class)->destroy (widget);
}

static void
gb_preferences_window_class_init (GbPreferencesWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkBindingSet *binding_set;

  object_class->constructed = gb_preferences_window_constructed;
  object_class->finalize = gb_preferences_window_finalize;

  widget_class->destroy = gb_preferences_window_destroy;
  widget_class->key_press_event = gb_preferences_window_key_press_event;

  gSignals [CLOSE] =
    g_signal_new_class_handler ("close",
                                G_TYPE_FROM_CLASS (klass),
                                (G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
                                G_CALLBACK (gb_preferences_window_close),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                0);

  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "close", 0);

  GB_WIDGET_CLASS_TEMPLATE (widget_class, "gb-preferences-window.ui");

  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesWindow, right_header_bar);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesWindow, search_bar);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesWindow, search_entry);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesWindow, stack);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesWindow, controls_stack);

  g_type_ensure (EGG_TYPE_SEARCH_BAR);
  g_type_ensure (GB_TYPE_PREFERENCES_PAGE_EDITOR);
  g_type_ensure (GB_TYPE_PREFERENCES_PAGE_GIT);
  g_type_ensure (GB_TYPE_PREFERENCES_PAGE_INSIGHT);
  g_type_ensure (GB_TYPE_PREFERENCES_PAGE_KEYBINDINGS);
  g_type_ensure (GB_TYPE_PREFERENCES_PAGE_LANGUAGE);
  g_type_ensure (GB_TYPE_PREFERENCES_PAGE_THEME);
}

static void
gb_preferences_window_init (GbPreferencesWindow *self)
{
  GList *pages;
  GList *iter;
  GtkWidget *controls;
  GtkAccelGroup *accel_group;

  gtk_widget_init_template (GTK_WIDGET (self));

  accel_group = gtk_accel_group_new ();
  gtk_widget_add_accelerator (GTK_WIDGET (self->search_bar), "reveal",
                              accel_group, GDK_KEY_f, GDK_CONTROL_MASK, 0);
  gtk_window_add_accel_group (GTK_WINDOW (self), accel_group);
  g_clear_object (&accel_group);

  pages = gtk_container_get_children (GTK_CONTAINER (self->stack));

  for (iter = pages; iter; iter = iter->next)
    {
      GbPreferencesPage *page = GB_PREFERENCES_PAGE (iter->data);

      controls = gb_preferences_page_get_controls (page);
      if (controls)
        gtk_container_add (GTK_CONTAINER (self->controls_stack), controls);
    }

  g_list_free (pages);
}
