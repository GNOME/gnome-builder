/* gbp-shellcmd-preferences-addin.c
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

#define G_LOG_DOMAIN "gbp-shellcmd-preferences-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "gbp-shellcmd-list.h"
#include "gbp-shellcmd-preferences-addin.h"

struct _GbpShellcmdPreferencesAddin
{
  GObject               parent_instance;
  IdePreferencesWindow *window;
  GSettings            *settings;
};

static void
handle_shellcmd_list (const char                   *page_name,
                      const IdePreferenceItemEntry *entry,
                      AdwPreferencesGroup          *group,
                      gpointer                      user_data)
{
  IdePreferencesWindow *window = user_data;

  IDE_ENTRY;

  g_assert (ide_str_equal0 (page_name, "commands"));
  g_assert (ADW_IS_PREFERENCES_GROUP (group));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  adw_preferences_group_set_header_suffix (group,
                                           g_object_new (GTK_TYPE_BUTTON,
                                                         "valign", GTK_ALIGN_CENTER,
                                                         "icon-name", "list-add-symbolic",
                                                         "css-classes", IDE_STRV_INIT ("flat"),
                                                         NULL));

  adw_preferences_group_add (group,
                             g_object_new (GBP_TYPE_SHELLCMD_LIST,
                                           NULL));

  IDE_EXIT;
}

static const IdePreferenceGroupEntry groups[] = {
  { "commands", "build", 0, N_("Build Commands") },
  { "commands", "run", 0, N_("Run Commands") },
};

static const IdePreferenceItemEntry items[] = {
  { "commands", "build", "list", 0, handle_shellcmd_list },
  { "commands", "run", "list", 0, handle_shellcmd_list },
};

static void
gbp_shellcmd_preferences_addin_load (IdePreferencesAddin  *addin,
                                     IdePreferencesWindow *window,
                                     IdeContext           *context)
{
  GbpShellcmdPreferencesAddin *self = (GbpShellcmdPreferencesAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  self->window = window;

  ide_preferences_window_add_groups (window, groups, G_N_ELEMENTS (groups), GETTEXT_PACKAGE);
  ide_preferences_window_add_items (window, items, G_N_ELEMENTS (items), window, NULL);

  IDE_EXIT;
}

static void
gbp_shellcmd_preferences_addin_unload (IdePreferencesAddin  *addin,
                                       IdePreferencesWindow *window,
                                       IdeContext           *context)
{
  GbpShellcmdPreferencesAddin *self = (GbpShellcmdPreferencesAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  self->window = NULL;

  IDE_EXIT;
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_shellcmd_preferences_addin_load;
  iface->unload = gbp_shellcmd_preferences_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpShellcmdPreferencesAddin, gbp_shellcmd_preferences_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
gbp_shellcmd_preferences_addin_class_init (GbpShellcmdPreferencesAddinClass *klass)
{
}

static void
gbp_shellcmd_preferences_addin_init (GbpShellcmdPreferencesAddin *self)
{
}
