/* ide-shortcut-bundle.h
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
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

typedef struct _IdeShortcut
{
  char                *id;
  char                *override;
  GtkShortcutTrigger  *trigger;
  TmplExpr            *when;
  GVariant            *args;
  GtkShortcutAction   *action;
  GtkPropagationPhase  phase;
} IdeShortcut;

G_DECLARE_FINAL_TYPE (IdeShortcutBundle, ide_shortcut_bundle, IDE, SHORTCUT_BUNDLE, GObject)

IdeShortcutBundle *ide_shortcut_bundle_new               (void);
IdeShortcutBundle *ide_shortcut_bundle_new_for_user      (GFile                *file);
gboolean           ide_shortcut_bundle_parse             (IdeShortcutBundle    *self,
                                                          GFile                *file,
                                                          GError              **error);
const GError      *ide_shortcut_bundle_error             (IdeShortcutBundle    *self);
gboolean           ide_shortcut_bundle_override          (IdeShortcutBundle    *bundle,
                                                          const char           *shortcut_id,
                                                          const char           *accelerator,
                                                          GError              **error);
gboolean           ide_shortcut_is_phase                 (GtkShortcut          *shortcut,
                                                          GtkPropagationPhase   phase);
gboolean           ide_shortcut_is_suppress              (GtkShortcut          *shortcut);
void               ide_shortcut_bundle_override_triggers (IdeShortcutBundle    *self,
                                                          GHashTable           *id_to_trigger);


G_END_DECLS
