/* gb-preferences-page-git.c
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

#include <libgit2-glib/ggit.h>

#include "gb-preferences-page-git.h"

struct _GbPreferencesPageGitPrivate
{
  GgitConfig *config;

  GtkEntry *git_author_name_entry;
  GtkEntry *git_author_email_entry;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbPreferencesPageGit, gb_preferences_page_git,
                            GTK_TYPE_BIN)

static void
on_author_name_changed (GtkEntry             *entry,
                        GbPreferencesPageGit *git)
{
  g_return_if_fail (GB_IS_PREFERENCES_PAGE_GIT (git));
  g_return_if_fail (GTK_IS_ENTRY (entry));

  ggit_config_set_string (git->priv->config, "user.name",
                          gtk_entry_get_text (entry), NULL);
}

static void
on_author_email_changed (GtkEntry             *entry,
                         GbPreferencesPageGit *git)
{
  g_return_if_fail (GB_IS_PREFERENCES_PAGE_GIT (git));
  g_return_if_fail (GTK_IS_ENTRY (entry));

  ggit_config_set_string (git->priv->config, "user.email",
                          gtk_entry_get_text (entry), NULL);
}

static void
gb_preferences_page_git_constructed (GObject *object)
{
  GbPreferencesPageGitPrivate *priv;
  GbPreferencesPageGit *git = (GbPreferencesPageGit *)object;
  const gchar *value;

  g_return_if_fail (GB_IS_PREFERENCES_PAGE_GIT (git));

  priv = git->priv;

  /* set current values from git */
  value = ggit_config_get_string (priv->config, "user.name", NULL);
  if (value)
    gtk_entry_set_text (priv->git_author_name_entry, value);
  value = ggit_config_get_string (priv->config, "user.email", NULL);
  if (value)
    gtk_entry_set_text (priv->git_author_email_entry, value);

  /* connect to changed signals to update values */
  g_signal_connect (priv->git_author_name_entry,
                    "changed",
                    G_CALLBACK (on_author_name_changed),
                    git);
  g_signal_connect (priv->git_author_email_entry,
                    "changed",
                    G_CALLBACK (on_author_email_changed),
                    git);

  G_OBJECT_CLASS (gb_preferences_page_git_parent_class)->constructed (object);
}

static void
gb_preferences_page_git_finalize (GObject *object)
{
  GbPreferencesPageGitPrivate *priv = GB_PREFERENCES_PAGE_GIT (object)->priv;

  g_clear_object (&priv->config);

  G_OBJECT_CLASS (gb_preferences_page_git_parent_class)->finalize (object);
}

static void
gb_preferences_page_git_class_init (GbPreferencesPageGitClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_preferences_page_git_constructed;
  object_class->finalize = gb_preferences_page_git_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/builder/ui/gb-preferences-page-git.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesPageGit, git_author_name_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GbPreferencesPageGit, git_author_email_entry);
}

static void
gb_preferences_page_git_init (GbPreferencesPageGit *self)
{
  self->priv = gb_preferences_page_git_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  self->priv->config = ggit_config_new_default (NULL);
}
