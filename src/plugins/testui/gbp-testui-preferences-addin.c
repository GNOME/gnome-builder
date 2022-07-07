/* gbp-testui-preferences-addin.c
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

#define G_LOG_DOMAIN "gbp-testui-preferences-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "gbp-testui-preferences-addin.h"

struct _GbpTestuiPreferencesAddin
{
  GObject parent_instance;
};

static const IdePreferenceGroupEntry groups[] = {
  { "application", "unit-tests", 100, N_("Unit Tests") },
};

static void
gbp_testui_preferences_addin_load (IdePreferencesAddin  *addin,
                                   IdePreferencesWindow *window,
                                   IdeContext           *context)
{
  g_autofree char *project_id = NULL;
  g_autofree char *project_settings_path = NULL;

  static IdePreferenceItemEntry items[] = {
    { "application", "unit-tests", "pipeline", 0, ide_preferences_window_check,
      N_("Build Pipeline"),
      N_("Run unit tests from within the build pipeline environment."),
      "org.gnome.builder.project", NULL, "unit-test-locality", "'pipeline'" },

    { "application", "unit-tests", "runtime", 0, ide_preferences_window_check,
      N_("As Application"),
      N_("Run unit tests with access to display and other runtime environment features."),
      "org.gnome.builder.project", NULL, "unit-test-locality", "'runtime'" },
  };

  g_assert (GBP_IS_TESTUI_PREFERENCES_ADDIN (addin));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (ide_preferences_window_get_mode (window) != IDE_PREFERENCES_MODE_PROJECT)
    return;

  project_id = ide_context_dup_project_id (context);
  project_settings_path = g_strdup_printf ("/org/gnome/builder/projects/%s/", project_id);

  for (guint i = 0; i < G_N_ELEMENTS (items); i++)
    items[i].path = project_settings_path;

  ide_preferences_window_add_groups (window, groups, G_N_ELEMENTS (groups), NULL);
  ide_preferences_window_add_items (window, items, G_N_ELEMENTS (items), window, NULL);
}

static void
gbp_testui_preferences_addin_unload (IdePreferencesAddin  *addin,
                                     IdePreferencesWindow *window,
                                     IdeContext           *context)
{
  g_assert (GBP_IS_TESTUI_PREFERENCES_ADDIN (addin));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));
  g_assert (!context || IDE_IS_CONTEXT (context));

}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_testui_preferences_addin_load;
  iface->unload = gbp_testui_preferences_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpTestuiPreferencesAddin, gbp_testui_preferences_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
gbp_testui_preferences_addin_class_init (GbpTestuiPreferencesAddinClass *klass)
{
}

static void
gbp_testui_preferences_addin_init (GbpTestuiPreferencesAddin *self)
{
}
