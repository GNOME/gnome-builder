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

  self->completion_id = dzl_preferences_add_switch (preferences,
                                                    "completion",
                                                    "providers",
                                                    "org.gnome.builder.extension-type",
                                                    "enabled",
                                                    "/org/gnome/builder/extension-types/clang-plugin/IdeCompletionProvider/",
                                                    NULL,
                                                    _("Suggest completions using Clang"),
                                                    _("Use Clang to suggest completions for C and C++ languages"),
                                                    NULL,
                                                    20);

  dzl_preferences_add_list_group (preferences, "completion", "clang", _("Clang Options"), GTK_SELECTION_NONE, 300);

  self->parens_id = dzl_preferences_add_switch (preferences,
                                                "completion",
                                                "clang",
                                                "org.gnome.builder.clang",
                                                "complete-parens",
                                                NULL,
                                                NULL,
                                                _("Complete Parenthesis"),
                                                _("Include parenthesis when completing clang proposals"),
                                                NULL,
                                                0);

  self->params_id = dzl_preferences_add_switch (preferences,
                                                "completion",
                                                "clang",
                                                "org.gnome.builder.clang",
                                                "complete-params",
                                                NULL,
                                                NULL,
                                                _("Complete Parameters"),
                                                _("Include parameters and types when completing clang proposals"),
                                                NULL,
                                                10);
}

static void
ide_clang_preferences_addin_unload (IdePreferencesAddin *addin,
                                    DzlPreferences      *preferences)
{
  IdeClangPreferencesAddin *self = (IdeClangPreferencesAddin *)addin;

  g_assert (IDE_IS_CLANG_PREFERENCES_ADDIN (addin));
  g_assert (DZL_IS_PREFERENCES (preferences));

  dzl_preferences_remove_id (preferences, self->completion_id);
  dzl_preferences_remove_id (preferences, self->diagnose_id);
  dzl_preferences_remove_id (preferences, self->parens_id);
  dzl_preferences_remove_id (preferences, self->params_id);
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = ide_clang_preferences_addin_load;
  iface->unload = ide_clang_preferences_addin_unload;
}
