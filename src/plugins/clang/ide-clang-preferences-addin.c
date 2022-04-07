/* ide-clang-preferences-addin.c
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

#define G_LOG_DOMAIN "ide-clang-preferences-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-code.h>
#include <libide-gui.h>

#include "ide-clang-preferences-addin.h"

struct _IdeClangPreferencesAddin
{
  GObject parent;
  guint   completion_id;
  guint   diagnose_id;
  guint   parens_id;
  guint   params_id;
};

static const IdePreferenceGroupEntry groups[] = {
  { "insight", "clang", 1000, N_("Clang") },
};

static const IdePreferenceItemEntry items[] = {
  { "insight", "diagnostics-providers", "clang", 0, ide_preferences_window_toggle,
    N_("Use Clang for Diagnostics"),
    N_("Clang will be queried for diagnostics within C, C++, and Objective-C sources"),
    "org.gnome.builder.extension-type",
    "/org/gnome/builder/extension-types/clang/IdeDiagnosticProvider/",
    "enabled" },

  { "insight", "completion-providers", "clang", 0, ide_preferences_window_toggle,
    N_("Use Clang for Completions"),
    N_("Clang will be queried for completions within C, C++, and Objective-C sources"),
    "org.gnome.builder.extension-type",
    "/org/gnome/builder/extension-types/clang/GtkSourceCompletionProvider/",
    "enabled" },

  { "insight", "clang", "parens", 0, ide_preferences_window_toggle,
    N_("Complete Parenthesis"),
    N_("Include parenthesis when completing clang proposals"),
    "org.gnome.builder.clang", NULL, "complete-parens" },

  { "insight", "clang", "params", 0, ide_preferences_window_toggle,
    N_("Complete Parameters"),
    N_("Include parameters and types when completing clang proposals"),
    "org.gnome.builder.clang", NULL, "complete-params" },
};

static void
ide_clang_preferences_addin_load (IdePreferencesAddin  *addin,
                                  IdePreferencesWindow *window)
{
  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_PREFERENCES_ADDIN (addin));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  ide_preferences_window_add_groups (window, groups, G_N_ELEMENTS (groups), NULL);
  ide_preferences_window_add_items (window, items, G_N_ELEMENTS (items), window, NULL);

  IDE_EXIT;
}

static void
ide_clang_preferences_addin_unload (IdePreferencesAddin  *addin,
                                    IdePreferencesWindow *window)
{
  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_PREFERENCES_ADDIN (addin));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  /* TODO: Remove gsettings switches */

  IDE_EXIT;
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = ide_clang_preferences_addin_load;
  iface->unload = ide_clang_preferences_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeClangPreferencesAddin, ide_clang_preferences_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
ide_clang_preferences_addin_class_init (IdeClangPreferencesAddinClass *klass)
{
}

static void
ide_clang_preferences_addin_init (IdeClangPreferencesAddin *self)
{
}
