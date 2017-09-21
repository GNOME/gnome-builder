/* ide-clang-preferences-addin.c
 *
 * Copyright (C) 2016 Christian Hergert <christian@hergert.me>
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
#include <ide.h>

#include "ide-clang-preferences-addin.h"

struct _IdeClangPreferencesAddin
{
  GObject parent;
  guint   diagnose_id;
};

static void preferences_addin_iface_init (IdePreferencesAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeClangPreferencesAddin, ide_clang_preferences_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN,
                                               preferences_addin_iface_init))

static void
ide_clang_preferences_addin_class_init (IdeClangPreferencesAddinClass *klass)
{
}

static void
ide_clang_preferences_addin_init (IdeClangPreferencesAddin *self)
{
}

static void
ide_clang_preferences_addin_load (IdePreferencesAddin *addin,
                                  DzlPreferences      *preferences)
{
  IdeClangPreferencesAddin *self = (IdeClangPreferencesAddin *)addin;

  g_assert (IDE_IS_CLANG_PREFERENCES_ADDIN (addin));
  g_assert (DZL_IS_PREFERENCES (preferences));

  self->diagnose_id = dzl_preferences_add_switch (preferences,
                                                  "code-insight",
                                                  "diagnostics",
                                                  "org.gnome.builder.extension-type",
                                                  "enabled",
                                                  "/org/gnome/builder/extension-types/clang-plugin/IdeDiagnosticProvider/",
                                                  NULL,
                                                  _("Clang"),
                                                  _("Show errors and warnings provided by Clang"),
                                                  /* translators: keywords used when searching for preferences */
                                                  _("clang diagnostics warnings errors"),
                                                  50);
}

static void
ide_clang_preferences_addin_unload (IdePreferencesAddin *addin,
                                    DzlPreferences      *preferences)
{
  IdeClangPreferencesAddin *self = (IdeClangPreferencesAddin *)addin;

  g_assert (IDE_IS_CLANG_PREFERENCES_ADDIN (addin));
  g_assert (DZL_IS_PREFERENCES (preferences));

  dzl_preferences_remove_id (preferences, self->diagnose_id);
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = ide_clang_preferences_addin_load;
  iface->unload = ide_clang_preferences_addin_unload;
}
