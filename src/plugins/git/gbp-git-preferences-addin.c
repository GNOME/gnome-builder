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
#include "gbp-git-vcs.h"
#include "gbp-git-vcs-config.h"

struct _GbpGitPreferencesAddin
{
  GObject parent_instance;
};

typedef struct
{
  IdeVcsConfig *config;
  IdeVcsConfigType key;
} ConfigKeyState;

static void
config_key_state_free (gpointer data)
{
  ConfigKeyState *state = data;

  ide_clear_and_destroy_object (&state->config);
  g_slice_free (ConfigKeyState, state);
}

static void
entry_config_changed (GtkEditable    *editable,
                      ConfigKeyState *state)
{
  g_auto(GValue) value = G_VALUE_INIT;
  const char *text;

  g_assert (GTK_IS_EDITABLE (editable));
  g_assert (state != NULL);
  g_assert (IDE_IS_VCS_CONFIG (state->config));

  text = gtk_editable_get_text (editable);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, text);

  ide_vcs_config_set_config (state->config, state->key, &value);
}

static GtkWidget *
create_entry (IdeVcsConfig     *config,
              const char       *title,
              IdeVcsConfigType  type)
{
  g_auto(GValue) value = G_VALUE_INIT;
  AdwEntryRow *entry;
  const char *text = NULL;
  ConfigKeyState *state;

  g_assert (GBP_IS_GIT_VCS_CONFIG (config));

  g_value_init (&value, G_TYPE_STRING);
  ide_vcs_config_get_config (config, type, &value);
  text = g_value_get_string (&value);

  entry = g_object_new (ADW_TYPE_ENTRY_ROW,
                        "title", title,
                        "text", text,
                        NULL);

  state = g_slice_new (ConfigKeyState);
  state->config = g_object_ref (config);
  state->key = type;

  g_signal_connect_data (entry,
                         "changed",
                         G_CALLBACK (entry_config_changed),
                         state,
                         (GClosureNotify)config_key_state_free,
                         G_CONNECT_AFTER);

  return GTK_WIDGET (entry);
}

static void
create_entry_row (const char                   *page_name,
                  const IdePreferenceItemEntry *entry,
                  AdwPreferencesGroup          *group,
                  gpointer                      user_data)
{
  g_autoptr(IdeVcsConfig) config = NULL;
  IdePreferencesWindow *window = user_data;
  IdePreferencesMode mode;
  IdeContext *context;

  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  /* We should always have a context, even if we're showing
   * global preferences.
   */
  mode = ide_preferences_window_get_mode (window);
  context = ide_preferences_window_get_context (window);

  g_assert (IDE_IS_CONTEXT (context));

  if (mode == IDE_PREFERENCES_MODE_PROJECT)
    {
      IdeVcs *vcs = ide_vcs_from_context (context);

      if (!GBP_IS_GIT_VCS (vcs))
        return;

      if (!(config = ide_vcs_get_config (vcs)))
        return;

      gbp_git_vcs_config_set_global (GBP_GIT_VCS_CONFIG (config), FALSE);
    }
  else
    {
      config = g_object_new (GBP_TYPE_GIT_VCS_CONFIG,
                             "parent", context,
                             NULL);
    }

  g_assert (GBP_IS_GIT_VCS_CONFIG (config));

  if (g_strcmp0 (entry->name, "name") == 0)
    {
      adw_preferences_group_add (group, create_entry (config, _("Author"), IDE_VCS_CONFIG_FULL_NAME));
      return;
    }

  if (g_strcmp0 (entry->name, "email") == 0)
    {
      const char *title;
      GtkWidget *label;

      adw_preferences_group_add (group, create_entry (config, _("Email"), IDE_VCS_CONFIG_EMAIL));

      /* After the email row, we want to add a blurb about whether this
       * will affect the global or per-project settings.
       */
      if (mode == IDE_PREFERENCES_MODE_PROJECT)
        title = _("The Git configuration options above effect current project only.");
      else
        title = _("The Git configuration options above effect global defaults.");

      label = g_object_new (GTK_TYPE_LABEL,
                            "css-classes", IDE_STRV_INIT ("caption", "dim-label"),
                            "xalign", .0f,
                            "margin-top", 15,
                            "single-line-mode", TRUE,
                            "label", title,
                            NULL);
      adw_preferences_group_add (group, label);

      return;
    }
}

static const IdePreferencePageEntry pages[] = {
  { NULL, "sharing", "git", "builder-vcs-git-symbolic", 500, N_("Version Control") },
};

static const IdePreferenceGroupEntry groups[] = {
  { "git", "author", 0, N_("Authorship") },
};

static const IdePreferenceItemEntry items[] = {
  { "git", "author", "name", 0, create_entry_row, N_("Full Name") },
  { "git", "author", "email", 10, create_entry_row, N_("Email Address") },
};

static void
gbp_git_preferences_addin_load (IdePreferencesAddin  *addin,
                                IdePreferencesWindow *window,
                                IdeContext           *context)
{
  g_assert (GBP_IS_GIT_PREFERENCES_ADDIN (addin));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));
  g_assert (!context || IDE_IS_CONTEXT (context));

  /* We can only show Git information if we have a project open as we need
   * access to a gnome-builder-git daemon. If no context is available, then
   * that means we got here by showing preferences with --preferences or
   * soemthing like that. In that (unlikely) case, we have to bail.
   */
  if (context == NULL)
    return;

  ide_preferences_window_add_pages (window, pages, G_N_ELEMENTS (pages), NULL);
  ide_preferences_window_add_groups (window, groups, G_N_ELEMENTS (groups), NULL);
  ide_preferences_window_add_items (window, items, G_N_ELEMENTS (items), window, NULL);
}

static void
gbp_git_preferences_addin_unload (IdePreferencesAddin  *addin,
                                  IdePreferencesWindow *window,
                                  IdeContext           *context)
{
  g_assert (GBP_IS_GIT_PREFERENCES_ADDIN (addin));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));
  g_assert (!context || IDE_IS_CONTEXT (context));

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
