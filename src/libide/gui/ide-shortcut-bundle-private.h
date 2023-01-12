/* ide-shortcut-bundle.h
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

#include <gtk/gtk.h>
#include <tmpl-glib.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_SHORTCUT_BUNDLE (ide_shortcut_bundle_get_type())

typedef struct
{
  TmplExpr            *when;
  GVariant            *args;
  GtkShortcutAction   *action;
  GtkPropagationPhase  phase;
} IdeShortcut;

G_DECLARE_FINAL_TYPE (IdeShortcutBundle, ide_shortcut_bundle, IDE, SHORTCUT_BUNDLE, GObject)

IdeShortcutBundle *ide_shortcut_bundle_new   (void);
gboolean           ide_shortcut_bundle_parse (IdeShortcutBundle  *self,
                                              GFile              *file,
                                              GError            **error);
const GError      *ide_shortcut_bundle_error (IdeShortcutBundle  *self);

#define ide_shortcut_is_phase(obj,pha) \
  (g_object_get_data(G_OBJECT(obj), "PHASE") == pha)

G_END_DECLS
