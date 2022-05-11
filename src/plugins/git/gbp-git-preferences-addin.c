/* gbp-git-preferences-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-git-preferences-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "gbp-git-preferences-addin.h"

struct _GbpGitPreferencesAddin
{
  GObject parent_instance;
};

static void
create_entry (const char                   *page_name,
              const IdePreferenceItemEntry *entry,
              AdwPreferencesGroup          *group,
              gpointer                      user_data)
{
  IDE_TODO ("Git: setup bindings for author info");

  if (FALSE) {}
  else if (g_strcmp0 (entry->name, "name") == 0)
    {
      adw_preferences_group_add (group,
                                 g_object_new (ADW_TYPE_ENTRY_ROW,
                                               "title", entry->title,
                                               NULL));
    }
  else if (g_strcmp0 (entry->name, "email") == 0)
    {
      adw_preferences_group_add (group,
                                 g_object_new (ADW_TYPE_ENTRY_ROW,
                                               "title", entry->title,
                                               NULL));
    }
}

static const IdePreferencePageEntry pages[] = {
  { NULL, "sharing", "git", "builder-vcs-git-symbolic", 500, N_("Git Version Control") },
};

static const IdePreferenceGroupEntry groups[] = {
  { "git", "author", 0, N_("Authorship") },
};

static const IdePreferenceItemEntry items[] = {
  { "git", "author", "name", 0, create_entry, N_("Full Name") },
  { "git", "author", "email", 10, create_entry, N_("Email Address") },
};

static void
gbp_git_preferences_addin_load (IdePreferencesAddin  *addin,
                                IdePreferencesWindow *window,
                                IdeContext           *context)
{
  g_assert (GBP_IS_GIT_PREFERENCES_ADDIN (addin));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));
  g_assert (IDE_IS_CONTEXT (context));

  ide_preferences_window_add_pages (window, pages, G_N_ELEMENTS (pages), NULL);
  ide_preferences_window_add_groups (window, groups, G_N_ELEMENTS (groups), NULL);
  ide_preferences_window_add_items (window, items, G_N_ELEMENTS (items),
                                    g_object_ref (context), g_object_unref);
}

static void
gbp_git_preferences_addin_unload (IdePreferencesAddin  *addin,
                                  IdePreferencesWindow *window,
                                  IdeContext           *context)
{
  g_assert (GBP_IS_GIT_PREFERENCES_ADDIN (addin));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));
  g_assert (IDE_IS_CONTEXT (context));

}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_git_preferences_addin_load;
  iface->unload = gbp_git_preferences_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGitPreferencesAddin, gbp_git_preferences_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
gbp_git_preferences_addin_class_init (GbpGitPreferencesAddinClass *klass)
{
}

static void
gbp_git_preferences_addin_init (GbpGitPreferencesAddin *self)
{
}
