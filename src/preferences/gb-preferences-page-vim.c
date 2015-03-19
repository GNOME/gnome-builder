/* gb-preferences-page-vim.c
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
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

#define G_LOG_DOMAIN "prefs-page-editor"

#include <glib/gi18n.h>

#include "gb-preferences-page-vim.h"
#include "gb-widget.h"

struct _GbPreferencesPageVimPrivate
{
  GSettings             *editor_settings;

  /* Widgets owned by Template */
  GtkSwitch             *vim_mode_switch;

  /* Template widgets used for filtering */
  GtkWidget             *vim_container;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbPreferencesPageVim, gb_preferences_page_vim,
                            GB_TYPE_PREFERENCES_PAGE)

static void
gb_preferences_page_vim_constructed (GObject *object)
{
  GbPreferencesPageVimPrivate *priv;
  GbPreferencesPageVim *vim = (GbPreferencesPageVim *)object;
  GSimpleActionGroup *group;
  GAction *action;

  g_return_if_fail (GB_IS_PREFERENCES_PAGE_VIM (vim));

  priv = vim->priv;

  priv->editor_settings = g_settings_new ("org.gnome.builder.editor");

  group = g_simple_action_group_new ();

  action = g_settings_create_action (priv->editor_settings, "keybindings");
  g_action_map_add_action (G_ACTION_MAP (group), action);
  g_clear_object (&action);

  gtk_widget_insert_action_group (GTK_WIDGET (vim), "settings", G_ACTION_GROUP (group));
  g_clear_object (&group);
}

static void
gb_preferences_page_vim_finalize (GObject *object)
{
  GbPreferencesPageVimPrivate *priv = GB_PREFERENCES_PAGE_VIM (object)->priv;

  g_clear_object (&priv->editor_settings);

  G_OBJECT_CLASS (gb_preferences_page_vim_parent_class)->finalize (object);
}

static void
gb_preferences_page_vim_class_init (GbPreferencesPageVimClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_preferences_page_vim_finalize;
  object_class->constructed = gb_preferences_page_vim_constructed;

  GB_WIDGET_CLASS_TEMPLATE (widget_class, "gb-preferences-page-vim.ui");
  GB_WIDGET_CLASS_BIND_PRIVATE (widget_class, GbPreferencesPageVim, vim_mode_switch);
  GB_WIDGET_CLASS_BIND_PRIVATE (widget_class, GbPreferencesPageVim, vim_container);
}

static void
gb_preferences_page_vim_init (GbPreferencesPageVim *self)
{
  self->priv = gb_preferences_page_vim_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  /* To translators: This is a list of keywords for the preferences page */
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("vim modal"),
                                               self->priv->vim_container,
                                               self->priv->vim_mode_switch,
                                               NULL);
}
