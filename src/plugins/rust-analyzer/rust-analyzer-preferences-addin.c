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

#include "rust-analyzer-preferences-addin.h"
#include <glib/gi18n.h>

struct _RustAnalyzerPreferencesAddin
{
  IdeObject parent_instance;
  guint check_id;
  guint clippy_id;
};

static void preferences_addin_iface_init (IdePreferencesAddinInterface *iface);

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

static void
rust_analyzer_preferences_addin_load (IdePreferencesAddin *addin,
                                      DzlPreferences      *preferences)
{
  RustAnalyzerPreferencesAddin *self = (RustAnalyzerPreferencesAddin *)addin;

  g_return_if_fail (RUST_IS_ANALYZER_PREFERENCES_ADDIN (self));
  g_return_if_fail (DZL_IS_PREFERENCES (preferences));

  dzl_preferences_add_list_group (preferences, "code-insight", "rust-analyzer", _("Rust Analyzer: Cargo command for diagnostics"), GTK_SELECTION_NONE, 0);
  self->check_id = dzl_preferences_add_radio (preferences,
                                              "code-insight",
                                              "rust-analyzer",
                                              "org.gnome.builder.rust-analyzer",
                                              "cargo-command",
                                              NULL,
                                              "\"check\"",
                                              "Cargo check",
                                              _("the default cargo command"),
                                              NULL, 1);
  self->clippy_id = dzl_preferences_add_radio (preferences,
                                               "code-insight",
                                               "rust-analyzer",
                                               "org.gnome.builder.rust-analyzer",
                                               "cargo-command",
                                               NULL,
                                               "\"clippy\"",
                                               "Cargo clippy",
                                               _("clippy adds additional lints to catch common mistakes but is in general slower"),
                                               NULL, 2);
}

static void
rust_analyzer_preferences_addin_unload (IdePreferencesAddin *addin,
                                        DzlPreferences      *preferences)
{
  RustAnalyzerPreferencesAddin *self = (RustAnalyzerPreferencesAddin *)addin;

  g_return_if_fail (RUST_IS_ANALYZER_PREFERENCES_ADDIN (self));
  g_return_if_fail (DZL_IS_PREFERENCES (preferences));

  dzl_preferences_remove_id (preferences, self->check_id);
  dzl_preferences_remove_id (preferences, self->clippy_id);
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = rust_analyzer_preferences_addin_load;
  iface->unload = rust_analyzer_preferences_addin_unload;
}
