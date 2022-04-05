/* gbp-vim-preferences-addin.c
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

#define G_LOG_DOMAIN "gbp-vim-preferences-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "gbp-vim-preferences-addin.h"

struct _GbpVimPreferencesAddin
{
  GObject parent_instance;
};

static const IdePreferenceItemEntry items[] = {
  { "keyboard", "keybindings", "vim", 0, ide_preferences_window_check,
    N_("Vim"),
    N_("Emulate keyboard shortcuts from Vim"),
    "org.gnome.builder.editor", NULL, "keybindings", "'vim'" },
};

static void
gbp_vim_preferences_addin_load (IdePreferencesAddin  *addin,
                                IdePreferencesWindow *window)
{
  GbpVimPreferencesAddin *self = (GbpVimPreferencesAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_VIM_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  ide_preferences_window_add_items (window, items, G_N_ELEMENTS (items), window, NULL);

  IDE_EXIT;
}

static void
gbp_vim_preferences_addin_unload (IdePreferencesAddin  *addin,
                                  IdePreferencesWindow *window)
{
  GbpVimPreferencesAddin *self = (GbpVimPreferencesAddin *)addin;

  g_assert (GBP_IS_VIM_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_vim_preferences_addin_load;
  iface->unload = gbp_vim_preferences_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpVimPreferencesAddin, gbp_vim_preferences_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
gbp_vim_preferences_addin_class_init (GbpVimPreferencesAddinClass *klass)
{
}

static void
gbp_vim_preferences_addin_init (GbpVimPreferencesAddin *self)
{
}
