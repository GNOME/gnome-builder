/* ide-ctags-preferences-addin.c
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

#define G_LOG_DOMAIN "ide-ctags-preferences-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-gui.h>

#include "ide-ctags-preferences-addin.h"

struct _IdeCtagsPreferencesAddin
{
  GObject parent_instance;
  guint enabled_id;
};

static void
ide_ctags_preferences_addin_load (IdePreferencesAddin *addin,
                                  DzlPreferences      *prefs)
{
  IdeCtagsPreferencesAddin *self = (IdeCtagsPreferencesAddin *)addin;

  g_assert (IDE_IS_CTAGS_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (prefs));

  self->enabled_id =
    dzl_preferences_add_switch (prefs,
                                "completion",
                                "providers",
                                "org.gnome.builder.extension-type",
                                "enabled",
                                "/org/gnome/builder/extension-types/ctags-plugin/IdeCompletionProvider/",
                                NULL,
                                _("Suggest completions using Ctags"),
                                _("Use Ctags to suggest completions for a variety of languages"),
                                NULL,
                                40);
}

static void
ide_ctags_preferences_addin_unload (IdePreferencesAddin *addin,
                                    DzlPreferences      *prefs)
{
  IdeCtagsPreferencesAddin *self = (IdeCtagsPreferencesAddin *)addin;

  g_assert (IDE_IS_CTAGS_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (prefs));

  dzl_preferences_remove_id (prefs, self->enabled_id);
}

static void
prefs_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = ide_ctags_preferences_addin_load;
  iface->unload = ide_ctags_preferences_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (IdeCtagsPreferencesAddin, ide_ctags_preferences_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, prefs_addin_iface_init))

static void
ide_ctags_preferences_addin_class_init (IdeCtagsPreferencesAddinClass *klass)
{
}

static void
ide_ctags_preferences_addin_init (IdeCtagsPreferencesAddin *self)
{
}
