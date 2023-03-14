/* gbp-shortcutui-shortcut.h
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GBP_TYPE_SHORTCUTUI_SHORTCUT (gbp_shortcutui_shortcut_get_type())

G_DECLARE_FINAL_TYPE (GbpShortcutuiShortcut, gbp_shortcutui_shortcut, GBP, SHORTCUTUI_SHORTCUT, GObject)

GbpShortcutuiShortcut *gbp_shortcutui_shortcut_new             (GtkShortcut                  *shortcut,
                                                                const char                   *page,
                                                                const char                   *group);
gboolean               gbp_shortcutui_shortcut_has_override    (GbpShortcutuiShortcut        *self);
char                  *gbp_shortcutui_shortcut_dup_accelerator (GbpShortcutuiShortcut        *self);
void                   gbp_shortcutui_shortcut_set_accelerator (GbpShortcutuiShortcut        *self,
                                                                const char                   *accelerator);
const char            *gbp_shortcutui_shortcut_get_title       (GbpShortcutuiShortcut        *self);
const char            *gbp_shortcutui_shortcut_get_subtitle    (GbpShortcutuiShortcut        *self);
const char            *gbp_shortcutui_shortcut_get_page        (GbpShortcutuiShortcut        *self);
const char            *gbp_shortcutui_shortcut_get_group       (GbpShortcutuiShortcut        *self);
gboolean               gbp_shortcutui_shortcut_override        (GbpShortcutuiShortcut        *self,
                                                                const char                   *accelerator,
                                                                GError                      **error);
int                    gbp_shortcutui_shortcut_compare         (const GbpShortcutuiShortcut  *a,
                                                                const GbpShortcutuiShortcut  *b);

G_END_DECLS
