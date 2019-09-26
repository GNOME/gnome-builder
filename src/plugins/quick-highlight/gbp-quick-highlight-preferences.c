/* gbp-quick-highlight-preferences.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-quick-highlight-preferences"

#include <glib/gi18n.h>

#include "gbp-quick-highlight-preferences.h"

struct _GbpQuickHighlightPreferences
{
  GObject parent_instance;
  guint   enable_switch;
};

static void
gbp_quick_highlight_preferences_load (IdePreferencesAddin *addin,
                                      DzlPreferences      *preferences)
{
  GbpQuickHighlightPreferences *self = (GbpQuickHighlightPreferences *)addin;

  g_assert (IDE_IS_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (preferences));

  self->enable_switch =
    dzl_preferences_add_switch (preferences,
                                "editor",
                                "highlight",
                                "org.gnome.builder.extension-type",
                                "enabled",
                                "/org/gnome/builder/extension-types/quick-highlight/IdeEditorViewAddin/",
                                NULL,
                                _("Words matching selection"),
                                _("Highlight all occurrences of words matching the current selection"),
                                /* Translators: the following are keywords used for searching to locate this preference */
                                _("quick highlight words matching current selection"),
                                10);
}

static void
gbp_quick_highlight_preferences_unload (IdePreferencesAddin *addin,
                                        DzlPreferences      *preferences)
{
  GbpQuickHighlightPreferences *self = (GbpQuickHighlightPreferences *)addin;

  g_assert (IDE_IS_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (preferences));

  dzl_preferences_remove_id (preferences, self->enable_switch);
  self->enable_switch = 0;
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_quick_highlight_preferences_load;
  iface->unload = gbp_quick_highlight_preferences_unload;
}

G_DEFINE_TYPE_EXTENDED (GbpQuickHighlightPreferences, gbp_quick_highlight_preferences, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
gbp_quick_highlight_preferences_class_init (GbpQuickHighlightPreferencesClass *klass)
{
}

static void
gbp_quick_highlight_preferences_init (GbpQuickHighlightPreferences *self)
{
}
