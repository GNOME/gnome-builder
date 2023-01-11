/* gbp-shortcutui-action.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define GBP_TYPE_SHORTCUTUI_ACTION (gbp_shortcutui_action_get_type())

G_DECLARE_FINAL_TYPE (GbpShortcutuiAction, gbp_shortcutui_action, GBP, SHORTCUTUI_ACTION, GObject)

const char *gbp_shortcutui_action_get_accelerator (const GbpShortcutuiAction *self);
const char *gbp_shortcutui_action_get_page        (const GbpShortcutuiAction *self);
const char *gbp_shortcutui_action_get_group       (const GbpShortcutuiAction *self);
int         gbp_shortcutui_action_compare         (const GbpShortcutuiAction *a,
                                                   const GbpShortcutuiAction *b);
gboolean    gbp_shortcutui_action_is_same_group   (const GbpShortcutuiAction *a,
                                                   const GbpShortcutuiAction *b);

G_END_DECLS
