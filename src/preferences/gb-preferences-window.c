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

/*
 * TODO: We will probably have to split all the preferences stuff out into
 *       child widgets just to keep things under control.
 *       Feel free to do that if you beat me to it.
 */

#include <glib/gi18n.h>
#include <libgit2-glib/ggit.h>

#include "gb-preferences-window.h"
#include "gb-sidebar.h"

struct _GbPreferencesWindowPrivate
{
  GtkHeaderBar    *right_header_bar;
  GtkSearchEntry  *search_entry;
  GtkSearchBar    *search_bar;
  GtkStack        *stack;

  GtkSwitch       *restore_insert_mark_switch;
  GtkSwitch       *vim_switch;
  GtkSwitch       *word_completion_switch;

  GtkEntry        *git_author_name_entry;
  GtkEntry        *git_author_email_entry;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbPreferencesWindow, gb_preferences_window,
                            GTK_TYPE_WINDOW)

GtkWidget *
gb_preferences_window_new (void)
{
  return g_object_new (GB_TYPE_PREFERENCES_WINDOW, NULL);
}

static void
gb_preferences_window_section_changed (GtkStack            *stack,
                                       GParamSpec          *pspec,
                                       GbPreferencesWindow *window)
{
  GtkWidget *visible_child;
  gchar *title = NULL;

  g_return_if_fail (GTK_IS_STACK (stack));
  g_return_if_fail (GB_IS_PREFERENCES_WINDOW (window));

  visible_child = gtk_stack_get_visible_child (stack);
  if (visible_child)
    gtk_container_child_get (GTK_CONTAINER (stack), visible_child,
                             "title", &title,
                             NULL);

  gtk_header_bar_set_title (window->priv->right_header_bar, title);

  g_free (title);
}

static void
load_editor (GbPreferencesWindow *window)
{
  GbPreferencesWindowPrivate *priv;
  GSettings *settings;

  g_return_if_fail (GB_IS_PREFERENCES_WINDOW (window));

  priv = window->priv;

  settings = g_settings_new ("org.gnome.builder.editor");

  g_settings_bind (settings, "vim-mode",
                   priv->vim_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings, "restore-insert-mark",
                   priv->restore_insert_mark_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings, "word-completion",
                   priv->word_completion_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_object_unref (settings);
}

static void
load_git (GbPreferencesWindow *window)
{
  GgitConfig *config;
  const gchar *value;

  g_return_if_fail (GB_IS_PREFERENCES_WINDOW (window));

  config = ggit_config_new_default (NULL);
  if (!config)
    return;

  /*
   * TODO: These should be bound to a config wrapper object that will sync
   *       the values back to the underlying config.
   */

  value = ggit_config_get_string (config, "user.name", NULL);
  if (value)
    gtk_entry_set_text (window->priv->git_author_name_entry, value);

  value = ggit_config_get_string (config, "user.email", NULL);
  if (value)
    gtk_entry_set_text (window->priv->git_author_email_entry, value);

  g_object_unref (config);
}

static void
gb_preferences_window_constructed (GObject *object)
{
  GbPreferencesWindow *window = (GbPreferencesWindow *)object;

  G_OBJECT_CLASS (gb_preferences_window_parent_class)->constructed (object);

  gtk_search_bar_connect_entry (window->priv->search_bar,
                                GTK_ENTRY (window->priv->search_entry));

  g_signal_connect (window->priv->stack,
                    "notify::visible-child",
                    G_CALLBACK (gb_preferences_window_section_changed),
                    window);
  gb_preferences_window_section_changed (window->priv->stack, NULL, window);

  load_editor (window);
  load_git (window);
}

static void
gb_preferences_window_finalize (GObject *object)
{
  G_OBJECT_CLASS (gb_preferences_window_parent_class)->finalize (object);
}

static void
gb_preferences_window_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_preferences_window_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_preferences_window_class_init (GbPreferencesWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_preferences_window_constructed;
  object_class->finalize = gb_preferences_window_finalize;
  object_class->get_property = gb_preferences_window_get_property;
  object_class->set_property = gb_preferences_window_set_property;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-preferences-window.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesWindow, git_author_email_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesWindow, git_author_name_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesWindow, restore_insert_mark_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesWindow, right_header_bar);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesWindow, search_bar);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesWindow, search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesWindow, stack);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesWindow, vim_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesWindow, word_completion_switch);

  g_type_ensure (GB_TYPE_SIDEBAR);
}

static void
gb_preferences_window_init (GbPreferencesWindow *self)
{
  self->priv = gb_preferences_window_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}
