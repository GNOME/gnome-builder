/* gbp-copyright-preferences-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
 * Copyright 2022 Tristan Partin <tristan@partin.io>
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

#define G_LOG_DOMAIN "gbp-copyright-preferences-addin"

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>

#include <libide-gui.h>
#include <libide-sourceview.h>

#include "gbp-copyright-preferences-addin.h"

#define SCHEMA_ID "org.gnome.builder.plugins.copyright"

struct _GbpCopyrightPreferencesAddin
{
  GObject parent_instance;
};

static const IdePreferenceItemEntry preference_items[] = {
  { .page = "editing", .group = "formatting", .name = "update-copyright", .priority = 0,
    .callback = ide_preferences_window_toggle, .title = N_("Update Copyright"),
    .subtitle = N_("Automatically update copyright headers when saving a file"),
    .schema_id = SCHEMA_ID, .key = "update-on-save",
  }
};

static void
gbp_copyright_preferences_addin_load (IdePreferencesAddin  *addin,
                                     IdePreferencesWindow *window,
                                     IdeContext           *context)
{
  GbpCopyrightPreferencesAddin *self = GBP_COPYRIGHT_PREFERENCES_ADDIN (addin);

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_COPYRIGHT_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));
  g_assert (!context || IDE_IS_CONTEXT (context));

  ide_preferences_window_add_items (window, preference_items, G_N_ELEMENTS (preference_items), NULL, NULL);

  IDE_EXIT;
}

static void
preferences_addin_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_copyright_preferences_addin_load;
}

G_DEFINE_TYPE_WITH_CODE (GbpCopyrightPreferencesAddin, gbp_copyright_preferences_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_init))

static void
gbp_copyright_preferences_addin_class_init (GbpCopyrightPreferencesAddinClass *klass)
{
}

static void
gbp_copyright_preferences_addin_init (GbpCopyrightPreferencesAddin *self)
{
}
