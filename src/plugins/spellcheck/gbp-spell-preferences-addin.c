/* gbp-spell-preferences-addin.c
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

#define G_LOG_DOMAIN "gbp-spell-preferences-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "gbp-spell-preferences-addin.h"

struct _GbpSpellPreferencesAddin
{
  GObject parent_instance;
};

static const IdePreferenceGroupEntry groups[] = {
  { "editing", "spelling",      0, N_("Spelling") },
};

static const IdePreferenceItemEntry items[] = {
  { "editing", "spelling", "enable-spellcheck", 0, ide_preferences_window_toggle,
    N_("Check Spelling"),
    N_("Automatically check spelling as you type"),
    "org.gnome.builder.spelling", NULL, "check-spelling" },
};

static void
gbp_spell_preferences_addin_load (IdePreferencesAddin  *addin,
                                  IdePreferencesWindow *window)
{
  GbpSpellPreferencesAddin *self = (GbpSpellPreferencesAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  ide_preferences_window_add_groups (window, groups, G_N_ELEMENTS (groups), NULL);
  ide_preferences_window_add_items (window, items, G_N_ELEMENTS (items), window, NULL);

  IDE_EXIT;
}

static void
gbp_spell_preferences_addin_unload (IdePreferencesAddin  *addin,
                                    IdePreferencesWindow *window)
{
  GbpSpellPreferencesAddin *self = (GbpSpellPreferencesAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  IDE_EXIT;
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_spell_preferences_addin_load;
  iface->unload = gbp_spell_preferences_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSpellPreferencesAddin, gbp_spell_preferences_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
gbp_spell_preferences_addin_class_init (GbpSpellPreferencesAddinClass *klass)
{
}

static void
gbp_spell_preferences_addin_init (GbpSpellPreferencesAddin *self)
{
}
