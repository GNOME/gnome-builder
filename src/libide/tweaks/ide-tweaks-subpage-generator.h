/* ide-tweaks-subpage-generator.h
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

#include "ide-tweaks-item.h"

G_BEGIN_DECLS

#define IDE_TYPE_TWEAKS_SUBPAGE_GENERATOR (ide_tweaks_subpage_generator_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeTweaksSubpageGenerator, ide_tweaks_subpage_generator, IDE, TWEAKS_SUBPAGE_GENERATOR, IdeTweaksItem)

struct _IdeTweaksSubpageGeneratorClass
{
  IdeTweaksItemClass parent_class;

  void (*populate) (IdeTweaksSubpageGenerator *self);
};

IDE_AVAILABLE_IN_ALL
IdeTweaksSubpageGenerator *ide_tweaks_subpage_generator_new      (void);
IDE_AVAILABLE_IN_ALL
void                       ide_tweaks_subpage_generator_populate (IdeTweaksSubpageGenerator *self);

G_END_DECLS
