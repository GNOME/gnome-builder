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

#include <libide-gui.h>

#include "gbp-quick-highlight-preferences.h"

struct _GbpQuickHighlightPreferences
{
  GObject parent_instance;
};

static const IdePreferenceGroupEntry groups[] = {
  { "editing", "quick-highlight", 300, N_("Highlighting") },
};

static const IdePreferenceItemEntry items[] = {
  { "editing", "quick-highlight", "enabled", 0, ide_preferences_window_toggle,
    N_("Highlight Words Matching Selection"),
    N_("Highlight all occurrences of words matching the current selection"),
    "org.gnome.builder.extension-type",
    "/org/gnome/builder/extension-types/quick-highlight/IdeEditorPageAddin/",
    "enabled" },

  { "editing", "quick-highlight", "min-length", 0, ide_preferences_window_spin,
    N_("Minimum Length for Highlight"),
    N_("Highlight words matching at least this number of characters"),
    "org.gnome.builder.editor",
    NULL,
    "min-char-selected" },
};

static void
gbp_quick_highlight_preferences_load (IdePreferencesAddin  *addin,
                                      IdePreferencesWindow *window,
                                      IdeContext           *context)
{
  GbpQuickHighlightPreferences *self = (GbpQuickHighlightPreferences *)addin;

  g_assert (IDE_IS_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  ide_preferences_window_add_groups (window, groups, G_N_ELEMENTS (groups), NULL);
  ide_preferences_window_add_items (window, items, G_N_ELEMENTS (items), window, NULL);
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_quick_highlight_preferences_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpQuickHighlightPreferences, gbp_quick_highlight_preferences, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
gbp_quick_highlight_preferences_class_init (GbpQuickHighlightPreferencesClass *klass)
{
}

static void
gbp_quick_highlight_preferences_init (GbpQuickHighlightPreferences *self)
{
}
