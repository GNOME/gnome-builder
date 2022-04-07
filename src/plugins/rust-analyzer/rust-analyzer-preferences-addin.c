/* rust-analyzer-preferences-addin.c
 *
 * Copyright 2020 Günther Wagner <info@gunibert.de>
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

#define G_LOG_DOMAIN "rust-analyzer-preferences-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "rust-analyzer-preferences-addin.h"

struct _RustAnalyzerPreferencesAddin
{
  IdeObject parent_instance;
};

static const IdePreferenceGroupEntry groups[] = {
  { "insight", "rust-analyer", 2000, N_("Rust Analyzer") },
};

static const IdePreferenceItemEntry items[] = {
  { "insight", "rust-analyzer", "cargo-command-check", 0, ide_preferences_window_check,
    N_("Cargo Check"),
    N_("Run “cargo check” as the default cargo command"),
    "org.gnome.builder.rust-analyzer", NULL, "cargo-command", "'check'" },

  { "insight", "rust-analyzer", "cargo-command-clippy", 0, ide_preferences_window_check,
    N_("Cargo Clippy"),
    N_("Run “cargo clippy” as the default cargo command"),
    "org.gnome.builder.rust-analyzer", NULL, "cargo-command", "'clippy'" },
};

static void
rust_analyzer_preferences_addin_load (IdePreferencesAddin  *addin,
                                      IdePreferencesWindow *window)
{
  RustAnalyzerPreferencesAddin *self = (RustAnalyzerPreferencesAddin *)addin;

  IDE_ENTRY;

  g_return_if_fail (RUST_IS_ANALYZER_PREFERENCES_ADDIN (self));
  g_return_if_fail (IDE_IS_PREFERENCES_WINDOW (window));

  ide_preferences_window_add_groups (window, groups, G_N_ELEMENTS (groups), NULL);
  ide_preferences_window_add_items (window, items, G_N_ELEMENTS (items), window, NULL);

  IDE_EXIT;
}

static void
rust_analyzer_preferences_addin_unload (IdePreferencesAddin  *addin,
                                        IdePreferencesWindow *window)
{
  IDE_ENTRY;

  g_return_if_fail (RUST_IS_ANALYZER_PREFERENCES_ADDIN (addin));
  g_return_if_fail (IDE_IS_PREFERENCES_WINDOW (window));

  IDE_EXIT;
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = rust_analyzer_preferences_addin_load;
  iface->unload = rust_analyzer_preferences_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (RustAnalyzerPreferencesAddin,
                               rust_analyzer_preferences_addin,
                               IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
rust_analyzer_preferences_addin_class_init (RustAnalyzerPreferencesAddinClass *klass)
{
}

static void
rust_analyzer_preferences_addin_init (RustAnalyzerPreferencesAddin *self)
{
}
