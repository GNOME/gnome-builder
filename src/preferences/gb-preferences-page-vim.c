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

struct _GbPreferencesPageVimPrivate
{
  GSettings             *editor_settings;
  GSettings             *vim_settings;

  /* Widgets owned by Template */
  GtkSpinButton         *scroll_off_spin;
  GtkSwitch             *vim_mode_switch;

  /* Template widgets used for filtering */
  GtkWidget             *scroll_off_container;
  GtkWidget             *vim_container;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbPreferencesPageVim, gb_preferences_page_vim,
                            GB_TYPE_PREFERENCES_PAGE)

static void
gb_preferences_page_vim_constructed (GObject *object)
{
  GbPreferencesPageVimPrivate *priv;
  GbPreferencesPageVim *vim = (GbPreferencesPageVim *)object;

  g_return_if_fail (GB_IS_PREFERENCES_PAGE_VIM (vim));

  priv = vim->priv;

  priv->editor_settings = g_settings_new ("org.gnome.builder.editor");
  priv->vim_settings = g_settings_new ("org.gnome.builder.editor.vim");

  g_settings_bind (priv->vim_settings, "scroll-off",
                   priv->scroll_off_spin, "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (priv->editor_settings, "vim-mode",
                   priv->vim_mode_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
gb_preferences_page_vim_finalize (GObject *object)
{
  GbPreferencesPageVimPrivate *priv = GB_PREFERENCES_PAGE_VIM (object)->priv;

  g_clear_object (&priv->editor_settings);
  g_clear_object (&priv->vim_settings);

  G_OBJECT_CLASS (gb_preferences_page_vim_parent_class)->finalize (object);
}

static void
gb_preferences_page_vim_class_init (GbPreferencesPageVimClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_preferences_page_vim_finalize;
  object_class->constructed = gb_preferences_page_vim_constructed;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-preferences-page-vim.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesPageVim, scroll_off_spin);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesPageVim, vim_mode_switch);

  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesPageVim, scroll_off_container);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesPageVim, vim_container);
}

static void
gb_preferences_page_vim_init (GbPreferencesPageVim *self)
{
  self->priv = gb_preferences_page_vim_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("lines margin scrolloff scroll off"),
                                               self->priv->scroll_off_container,
                                               self->priv->scroll_off_spin,
                                               NULL);

  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("vim modal"),
                                               self->priv->vim_container,
                                               self->priv->vim_mode_switch,
                                               NULL);
}
