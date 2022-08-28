/* ide-tweaks-binding.h
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

#if !defined (IDE_TWEAKS_INSIDE) && !defined (IDE_TWEAKS_COMPILATION)
# error "Only <libide-tweaks.h> can be included directly."
#endif

#include "ide-tweaks-item.h"

G_BEGIN_DECLS

#define IDE_TYPE_TWEAKS_BINDING (ide_tweaks_binding_get_type())

typedef gboolean (*IdeTweaksBindingTransform) (const GValue *from_value,
                                               GValue       *to_value,
                                               gpointer      user_data);

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeTweaksBinding, ide_tweaks_binding, IDE, TWEAKS_BINDING, IdeTweaksItem)

struct _IdeTweaksBindingClass
{
  IdeTweaksItemClass parent_class;

  void           (*changed)           (IdeTweaksBinding *self);
  gboolean       (*get_value)         (IdeTweaksBinding *self,
                                       GValue           *value);
  void           (*set_value)         (IdeTweaksBinding *self,
                                       const GValue     *value);
  GType          (*get_expected_type) (IdeTweaksBinding *self);
  GtkAdjustment *(*create_adjustment) (IdeTweaksBinding *self);
};

IDE_AVAILABLE_IN_ALL
void           ide_tweaks_binding_changed             (IdeTweaksBinding          *self);
IDE_AVAILABLE_IN_ALL
gboolean       ide_tweaks_binding_get_value           (IdeTweaksBinding          *self,
                                                       GValue                    *value);
IDE_AVAILABLE_IN_ALL
void           ide_tweaks_binding_set_value           (IdeTweaksBinding          *self,
                                                       const GValue              *value);
IDE_AVAILABLE_IN_ALL
void           ide_tweaks_binding_set_variant         (IdeTweaksBinding          *self,
                                                       GVariant                  *variant);
IDE_AVAILABLE_IN_ALL
char          *ide_tweaks_binding_dup_string          (IdeTweaksBinding          *self);
IDE_AVAILABLE_IN_ALL
void           ide_tweaks_binding_set_string          (IdeTweaksBinding          *self,
                                                       const char                *string);
IDE_AVAILABLE_IN_ALL
char         **ide_tweaks_binding_dup_strv            (IdeTweaksBinding          *self);
IDE_AVAILABLE_IN_ALL
void           ide_tweaks_binding_set_strv            (IdeTweaksBinding          *self,
                                                       const char * const        *strv);
IDE_AVAILABLE_IN_ALL
void           ide_tweaks_binding_bind                (IdeTweaksBinding          *self,
                                                       gpointer                   instance,
                                                       const char                *property_name);
IDE_AVAILABLE_IN_ALL
void           ide_tweaks_binding_bind_with_transform (IdeTweaksBinding          *self,
                                                       gpointer                   instance,
                                                       const char                *property_name,
                                                       IdeTweaksBindingTransform  get_transform,
                                                       IdeTweaksBindingTransform  set_transform,
                                                       gpointer                   user_data,
                                                       GDestroyNotify             notify);
IDE_AVAILABLE_IN_ALL
void           ide_tweaks_binding_unbind              (IdeTweaksBinding          *self);
IDE_AVAILABLE_IN_ALL
gboolean       ide_tweaks_binding_get_expected_type   (IdeTweaksBinding          *self,
                                                       GType                     *type);
IDE_AVAILABLE_IN_ALL
GtkAdjustment *ide_tweaks_binding_create_adjustment   (IdeTweaksBinding          *self);

G_END_DECLS
