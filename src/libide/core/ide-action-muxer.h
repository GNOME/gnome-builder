/* ide-action-muxer.h
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

#if !defined (IDE_CORE_INSIDE) && !defined (IDE_CORE_COMPILATION)
# error "Only <libide-core.h> can be included directly."
#endif

#include <gio/gio.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

typedef void (*IdeActionActivateFunc) (gpointer    instance,
                                       const char *action_name,
                                       GVariant   *param);

typedef struct _IdeAction
{
  const struct _IdeAction *next;
  const char              *name;
  GType                    owner;
  const GVariantType      *parameter_type;
  const GVariantType      *state_type;
  GParamSpec              *pspec;
  IdeActionActivateFunc    activate;
  guint                    position;
} IdeAction;

typedef struct _IdeActionMixin
{
  GObjectClass    *object_class;
  const IdeAction *actions;
  guint            n_actions;
} IdeActionMixin;

#define IDE_TYPE_ACTION_MUXER (ide_action_muxer_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeActionMuxer, ide_action_muxer, IDE, ACTION_MUXER, GObject)

IDE_AVAILABLE_IN_ALL
IdeActionMuxer  *ide_action_mixin_get_action_muxer        (gpointer               instance);
IDE_AVAILABLE_IN_ALL
void             ide_action_mixin_init                    (IdeActionMixin        *mixin,
                                                           GObjectClass          *object_class);
IDE_AVAILABLE_IN_ALL
void             ide_action_mixin_constructed             (const IdeActionMixin  *mixin,
                                                           gpointer               instance);
IDE_AVAILABLE_IN_ALL
void             ide_action_mixin_set_enabled             (gpointer               instance,
                                                           const char            *action,
                                                           gboolean               enabled);
IDE_AVAILABLE_IN_ALL
void             ide_action_mixin_install_action          (IdeActionMixin        *mixin,
                                                           const char            *action_name,
                                                           const char            *parameter_type,
                                                           IdeActionActivateFunc  activate);
IDE_AVAILABLE_IN_ALL
void             ide_action_mixin_install_property_action (IdeActionMixin        *mixin,
                                                           const char            *action_name,
                                                           const char            *property_name);
IDE_AVAILABLE_IN_ALL
IdeActionMuxer  *ide_action_muxer_new                     (void);
IDE_AVAILABLE_IN_ALL
void             ide_action_muxer_remove_all              (IdeActionMuxer        *self);
IDE_AVAILABLE_IN_ALL
void             ide_action_muxer_insert_action_group     (IdeActionMuxer        *self,
                                                           const char            *prefix,
                                                           GActionGroup          *action_group);
IDE_AVAILABLE_IN_ALL
void             ide_action_muxer_remove_action_group     (IdeActionMuxer        *self,
                                                           const char            *prefix);
IDE_AVAILABLE_IN_ALL
char           **ide_action_muxer_list_groups             (IdeActionMuxer        *self);
IDE_AVAILABLE_IN_ALL
GActionGroup    *ide_action_muxer_get_action_group        (IdeActionMuxer        *self,
                                                           const char            *prefix);
IDE_AVAILABLE_IN_ALL
void             ide_action_muxer_set_enabled             (IdeActionMuxer        *self,
                                                           const IdeAction       *action,
                                                           gboolean               enabled);
IDE_AVAILABLE_IN_ALL
void             ide_action_muxer_connect_actions         (IdeActionMuxer        *self,
                                                           gpointer               instance,
                                                           const IdeAction       *actions);

G_END_DECLS
