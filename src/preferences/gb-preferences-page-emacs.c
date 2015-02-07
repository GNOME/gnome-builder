/* gb-preferences-page-emacs.c
 *
 * Copyright (C) 2015 Roberto Majadas <roberto.majadas@openshine.com>
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

#include "gb-preferences-page-emacs.h"

struct _GbPreferencesPageEmacsPrivate
{
  GSettings             *editor_settings;

  /* Widgets owned by Template */
  GtkSwitch             *emacs_mode_switch;

  /* Template widgets used for filtering */
  GtkWidget             *emacs_container;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbPreferencesPageEmacs, gb_preferences_page_emacs,
                            GB_TYPE_PREFERENCES_PAGE)

static void
gb_preferences_page_emacs_constructed (GObject *object)
{
  GbPreferencesPageEmacsPrivate *priv;
  GbPreferencesPageEmacs *emacs = (GbPreferencesPageEmacs *)object;

  g_return_if_fail (GB_IS_PREFERENCES_PAGE_EMACS (emacs));

  priv = emacs->priv;

  priv->editor_settings = g_settings_new ("org.gnome.builder.editor");

  g_settings_bind (priv->editor_settings, "emacs-mode",
                   priv->emacs_mode_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
gb_preferences_page_emacs_finalize (GObject *object)
{
  GbPreferencesPageEmacsPrivate *priv = GB_PREFERENCES_PAGE_EMACS (object)->priv;

  g_clear_object (&priv->editor_settings);

  G_OBJECT_CLASS (gb_preferences_page_emacs_parent_class)->finalize (object);
}

static void
gb_preferences_page_emacs_class_init (GbPreferencesPageEmacsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_preferences_page_emacs_finalize;
  object_class->constructed = gb_preferences_page_emacs_constructed;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-preferences-page-emacs.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesPageEmacs, emacs_mode_switch);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesPageEmacs, emacs_container);
}

static void
gb_preferences_page_emacs_init (GbPreferencesPageEmacs *self)
{
  self->priv = gb_preferences_page_emacs_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("emacs modal"),
                                               self->priv->emacs_container,
                                               self->priv->emacs_mode_switch,
                                               NULL);
}
