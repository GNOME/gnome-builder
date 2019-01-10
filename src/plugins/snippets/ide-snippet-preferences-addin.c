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

static void
ide_snippet_preferences_addin_load (IdePreferencesAddin *addin,
                                    DzlPreferences      *prefs)
{
  IdeSnippetPreferencesAddin *self = (IdeSnippetPreferencesAddin *)addin;

  g_assert (IDE_IS_SNIPPET_PREFERENCES_ADDIN (addin));
  g_assert (DZL_IS_PREFERENCES (prefs));

  self->enabled_id =
    dzl_preferences_add_switch (prefs,
                                "completion",
                                "providers",
                                "org.gnome.builder.extension-type",
                                "enabled",
                                "/org/gnome/builder/extension-types/snippets-plugin/IdeCompletionProvider/",
                                NULL,
                                _("Suggest Completions from Snippets"),
                                _("Use registered snippets to suggest completion proposals"),
                                NULL,
                                10);
}

static void
ide_snippet_preferences_addin_unload (IdePreferencesAddin *addin,
                                      DzlPreferences      *prefs)
{
  IdeSnippetPreferencesAddin *self = (IdeSnippetPreferencesAddin *)addin;

  g_assert (IDE_IS_SNIPPET_PREFERENCES_ADDIN (addin));
  g_assert (DZL_IS_PREFERENCES (prefs));

  dzl_preferences_remove_id (prefs, self->enabled_id);
}

static void
prefs_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = ide_snippet_preferences_addin_load;
  iface->unload = ide_snippet_preferences_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (IdeSnippetPreferencesAddin, ide_snippet_preferences_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, prefs_addin_iface_init))

static void
ide_snippet_preferences_addin_class_init (IdeSnippetPreferencesAddinClass *klass)
{
}

static void
ide_snippet_preferences_addin_init (IdeSnippetPreferencesAddin *self)
{
}
