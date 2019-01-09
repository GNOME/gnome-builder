/* ide-gca-preferences-addin.c
 *
 * Copyright 2016-2019 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>
#include <libide-gui.h>

#include "ide-gca-preferences-addin.h"

static void preferences_addin_iface_init (IdePreferencesAddinInterface *iface);

struct _IdeGcaPreferencesAddin
{
  GObject parent;
  guint   pylint;
};

G_DEFINE_TYPE_EXTENDED (IdeGcaPreferencesAddin, ide_gca_preferences_addin, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN,
                                               preferences_addin_iface_init))

static void
ide_gca_preferences_addin_class_init (IdeGcaPreferencesAddinClass *klass)
{
}

static void
ide_gca_preferences_addin_init (IdeGcaPreferencesAddin *self)
{
}

static void
ide_gca_preferences_addin_load (IdePreferencesAddin *addin,
                                DzlPreferences      *preferences)
{
  IdeGcaPreferencesAddin *self = (IdeGcaPreferencesAddin *)addin;

  g_assert (IDE_IS_GCA_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (preferences));

  self->pylint = dzl_preferences_add_switch (preferences,
                                             "code-insight",
                                             "diagnostics",
                                             "org.gnome.builder.gnome-code-assistance",
                                             "enable-pylint",
                                             NULL,
                                             "false",
                                             _("Pylint"),
                                             _("Enable the use of pylint, which may execute code in your project"),
                                             /* translators: these are keywords used to search for preferences */
                                             _("pylint python lint code execute execution"),
                                             500);
}

static void
ide_gca_preferences_addin_unload (IdePreferencesAddin *addin,
                                  DzlPreferences      *preferences)
{
  IdeGcaPreferencesAddin *self = (IdeGcaPreferencesAddin *)addin;

  g_assert (IDE_IS_GCA_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (preferences));

  dzl_preferences_remove_id (preferences, self->pylint);
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = ide_gca_preferences_addin_load;
  iface->unload = ide_gca_preferences_addin_unload;
}
