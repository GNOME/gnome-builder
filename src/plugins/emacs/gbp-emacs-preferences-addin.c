/* gbp-emacs-preferences-addin.c
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

#define G_LOG_DOMAIN "gbp-emacs-preferences-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-gui.h>

#include "gbp-emacs-preferences-addin.h"

struct _GbpEmacsPreferencesAddin
{
  GObject parent_instance;
  guint   keybinding_id;
};

static void
gbp_emacs_preferences_addin_load (IdePreferencesAddin *addin,
                                  DzlPreferences      *preferences)
{
  GbpEmacsPreferencesAddin *self = (GbpEmacsPreferencesAddin *)addin;

  g_assert (GBP_IS_EMACS_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (preferences));

  self->keybinding_id = dzl_preferences_add_radio (preferences,
                                                   "keyboard",
                                                   "mode",
                                                   "org.gnome.builder.editor",
                                                   "keybindings",
                                                   NULL,
                                                   "\"emacs\"",
                                                   _("Emacs"),
                                                   _("Emulates the Emacs text editor"),
                                                   NULL,
                                                   10);
}

static void
gbp_emacs_preferences_addin_unload (IdePreferencesAddin *addin,
                                    DzlPreferences      *preferences)
{
  GbpEmacsPreferencesAddin *self = (GbpEmacsPreferencesAddin *)addin;

  g_assert (GBP_IS_EMACS_PREFERENCES_ADDIN (self));
  g_assert (DZL_IS_PREFERENCES (preferences));

  dzl_preferences_remove_id (preferences, self->keybinding_id);
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_emacs_preferences_addin_load;
  iface->unload = gbp_emacs_preferences_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpEmacsPreferencesAddin, gbp_emacs_preferences_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN,
                                                preferences_addin_iface_init))

static void
gbp_emacs_preferences_addin_class_init (GbpEmacsPreferencesAddinClass *klass)
{
}

static void
gbp_emacs_preferences_addin_init (GbpEmacsPreferencesAddin *self)
{
}
