/* ide-snippet-preferences-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-snippet-preferences-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-gui.h>

#include "ide-snippet-preferences-addin.h"

struct _IdeSnippetPreferencesAddin
{
  GObject parent_instance;
  guint enabled_id;
};

static const IdePreferenceGroupEntry groups[] = {
  { "insight", "snippets", 1000, N_("Snippets") },
};

static const IdePreferenceItemEntry items[] = {
  { "insight", "completion-providers", "enable-snippets", 0, ide_preferences_window_toggle,
    N_("Suggest Completions from Snippets"),
    N_("Use registered snippets to suggest completion proposals"),
    "org.gnome.builder.extension-type",
    "/org/gnome/builder/extension-types/snippets/GtkSourceCompletionProvider/",
    "enabled" }
};

static void
ide_snippet_preferences_addin_load (IdePreferencesAddin  *addin,
                                    IdePreferencesWindow *window)
{
  g_assert (IDE_IS_SNIPPET_PREFERENCES_ADDIN (addin));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  ide_preferences_window_add_groups (window, groups, G_N_ELEMENTS (groups), NULL);
  ide_preferences_window_add_items (window, items, G_N_ELEMENTS (items), window, NULL);
}

static void
ide_snippet_preferences_addin_unload (IdePreferencesAddin  *addin,
                                      IdePreferencesWindow *window)
{
  g_assert (IDE_IS_SNIPPET_PREFERENCES_ADDIN (addin));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));
}

static void
prefs_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = ide_snippet_preferences_addin_load;
  iface->unload = ide_snippet_preferences_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeSnippetPreferencesAddin, ide_snippet_preferences_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, prefs_addin_iface_init))

static void
ide_snippet_preferences_addin_class_init (IdeSnippetPreferencesAddinClass *klass)
{
}

static void
ide_snippet_preferences_addin_init (IdeSnippetPreferencesAddin *self)
{
}
