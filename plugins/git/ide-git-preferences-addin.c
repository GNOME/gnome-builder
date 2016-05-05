/* ide-git-preferences-addin.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

/* Semi public API */
#include "preferences/ide-preferences-entry.h"

#include "ide-git-preferences-addin.h"

struct _IdeGitPreferencesAddin
{
  GObject parent_instance;
};

static void preferences_addin_iface_init (IdePreferencesAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeGitPreferencesAddin, ide_git_preferences_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN,
                                               preferences_addin_iface_init))

static void
ide_git_preferences_addin_class_init (IdeGitPreferencesAddinClass *klass)
{
}

static void
ide_git_preferences_addin_init (IdeGitPreferencesAddin *self)
{
}

static gchar *
read_config_string (GgitConfig   *orig_config,
                    const gchar  *key,
                    GError      **error)
{
  GgitConfig *config;
  const gchar *value;
  gchar *ret;

  g_assert (GGIT_IS_CONFIG (orig_config));
  g_assert (key != NULL);

  config = ggit_config_snapshot (orig_config, error);
  if (config == NULL)
    return NULL;

  value = ggit_config_get_string (config, key, error);

  ret = value ? g_strdup (value) : NULL;

  g_clear_object (&config);

  return ret;
}

static void
author_changed_cb (IdePreferencesEntry *entry,
                   const gchar         *text,
                   GgitConfig          *config)
{
  g_assert (IDE_IS_PREFERENCES_ENTRY (entry));
  g_assert (text != NULL);
  g_assert (GGIT_IS_CONFIG (config));

  ggit_config_set_string (config, "user.name", text, NULL);
}

static void
email_changed_cb (IdePreferencesEntry *entry,
                  const gchar         *text,
                  GgitConfig          *config)
{
  g_assert (IDE_IS_PREFERENCES_ENTRY (entry));
  g_assert (text != NULL);
  g_assert (GGIT_IS_CONFIG (config));

  ggit_config_set_string (config, "user.email", text, NULL);
}

static void
register_git (IdePreferences *preferences)
{
  g_autofree gchar *author_text = NULL;
  g_autofree gchar *email_text = NULL;
  g_autoptr(GFile) global_file = NULL;
  GgitConfig *config;
  GtkSizeGroup *size_group;
  GtkWidget *author;
  GtkWidget *email;

  ide_preferences_add_page (preferences, "git", _("Version Control"), 600);

  if (!(global_file = ggit_config_find_global ()))
    {
      g_autofree gchar *path = NULL;

      path = g_build_filename (g_get_home_dir (), ".gitconfig", NULL);
      global_file = g_file_new_for_path (path);
    }

  config = ggit_config_new_from_file (global_file, NULL);
  g_object_set_data_full (G_OBJECT (preferences), "GGIT_CONFIG", config, g_object_unref);

  author_text = read_config_string (config, "user.name", NULL);
  author = g_object_new (IDE_TYPE_PREFERENCES_ENTRY,
                         "text", author_text ?: "",
                         "title", "Author",
                         "visible", TRUE,
                         NULL);
  g_signal_connect_object (author,
                           "changed",
                           G_CALLBACK (author_changed_cb),
                           config,
                           0);

  email_text = read_config_string (config, "user.email", NULL);
  email = g_object_new (IDE_TYPE_PREFERENCES_ENTRY,
                         "text", email_text ?: "",
                        "title", "Email",
                        "visible", TRUE,
                        NULL);
  g_signal_connect_object (email,
                           "changed",
                           G_CALLBACK (email_changed_cb),
                           config,
                           0);

  ide_preferences_add_list_group (preferences, "git", "attribution", _("Attribution"), 0);
  ide_preferences_add_custom (preferences, "git", "attribution", author, NULL, 0);
  ide_preferences_add_custom (preferences, "git", "attribution", email, NULL, 0);

  size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  gtk_size_group_add_widget (size_group, ide_preferences_entry_get_title_widget (IDE_PREFERENCES_ENTRY (author)));
  gtk_size_group_add_widget (size_group, ide_preferences_entry_get_title_widget (IDE_PREFERENCES_ENTRY (email)));
  g_clear_object (&size_group);
}


static void
ide_git_preferences_addin_load (IdePreferencesAddin *addin,
                                IdePreferences      *preferences)
{
  IdeGitPreferencesAddin *self = (IdeGitPreferencesAddin *)addin;

  g_assert (IDE_IS_GIT_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES (preferences));

  register_git (preferences);
}

static void
ide_git_preferences_addin_unload (IdePreferencesAddin *addin,
                                  IdePreferences      *preferences)
{
  IdeGitPreferencesAddin *self = (IdeGitPreferencesAddin *)addin;

  g_assert (IDE_IS_GIT_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES (preferences));

  /* TODO: Unregister preferences */
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = ide_git_preferences_addin_load;
  iface->unload = ide_git_preferences_addin_unload;
}
