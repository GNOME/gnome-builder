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

#include <glib/gi18n.h>
#include <libgit2-glib/ggit.h>

#include "gb-preferences-page-git.h"
#include "gb-widget.h"

struct _GbPreferencesPageGit
{
  GbPreferencesPage  parent_instance;

  GgitConfig        *config;
  GtkEntry          *git_author_name_entry;
  GtkEntry          *git_author_email_entry;
  GtkWidget         *name_label;
  GtkWidget         *email_label;
};

G_DEFINE_TYPE (GbPreferencesPageGit, gb_preferences_page_git,
               GB_TYPE_PREFERENCES_PAGE)

static void
on_author_name_changed (GtkEntry             *entry,
                        GbPreferencesPageGit *git)
{
  g_return_if_fail (GB_IS_PREFERENCES_PAGE_GIT (git));
  g_return_if_fail (GTK_IS_ENTRY (entry));

  ggit_config_set_string (git->config, "user.name",
                          gtk_entry_get_text (entry), NULL);
}

static void
on_author_email_changed (GtkEntry             *entry,
                         GbPreferencesPageGit *git)
{
  g_return_if_fail (GB_IS_PREFERENCES_PAGE_GIT (git));
  g_return_if_fail (GTK_IS_ENTRY (entry));

  ggit_config_set_string (git->config, "user.email",
                          gtk_entry_get_text (entry), NULL);
}

static void
gb_preferences_page_git_constructed (GObject *object)
{
  GbPreferencesPageGit *git = (GbPreferencesPageGit *)object;
  const gchar *value;

  g_return_if_fail (GB_IS_PREFERENCES_PAGE_GIT (git));

  /* set current values from git */
  value = ggit_config_get_string (git->config, "user.name", NULL);
  if (value)
    gtk_entry_set_text (git->git_author_name_entry, value);
  value = ggit_config_get_string (git->config, "user.email", NULL);
  if (value)
    gtk_entry_set_text (git->git_author_email_entry, value);

  /* connect to changed signals to update values */
  g_signal_connect (git->git_author_name_entry,
                    "changed",
                    G_CALLBACK (on_author_name_changed),
                    git);
  g_signal_connect (git->git_author_email_entry,
                    "changed",
                    G_CALLBACK (on_author_email_changed),
                    git);

  G_OBJECT_CLASS (gb_preferences_page_git_parent_class)->constructed (object);
}

static void
gb_preferences_page_git_finalize (GObject *object)
{
  GbPreferencesPageGit *self = GB_PREFERENCES_PAGE_GIT (object);

  g_clear_object (&self->config);

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

  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageGit, git_author_name_entry);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageGit, git_author_email_entry);

  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageGit, name_label);
  GB_WIDGET_CLASS_BIND (widget_class, GbPreferencesPageGit, email_label);
}

static void
gb_preferences_page_git_init (GbPreferencesPageGit *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->config = ggit_config_new_default (NULL);

  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("git author name surname"),
                                               self->name_label,
                                               self->git_author_name_entry,
                                               NULL);
  gb_preferences_page_set_keywords_for_widget (GB_PREFERENCES_PAGE (self),
                                               _("git author email mail address"),
                                               self->email_label,
                                               self->git_author_email_entry,
                                               NULL);
}
