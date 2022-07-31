/* ide-tweaks-section.c

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

#define G_LOG_DOMAIN "ide-tweaks-section"

#include "config.h"

#include "ide-tweaks-page.h"
#include "ide-tweaks-section.h"

struct _IdeTweaksSection
{
  IdeTweaksItem parent_instance;
};

G_DEFINE_FINAL_TYPE (IdeTweaksSection, ide_tweaks_section, IDE_TYPE_TWEAKS_ITEM)

static gboolean
ide_tweaks_section_accepts (IdeTweaksItem *item,
                            IdeTweaksItem *child)
{
  g_assert (IDE_IS_TWEAKS_ITEM (item));
  g_assert (IDE_IS_TWEAKS_ITEM (child));

  return IDE_IS_TWEAKS_PAGE (child);
}

static void
ide_tweaks_section_class_init (IdeTweaksSectionClass *klass)
{
  IdeTweaksItemClass *item_class = IDE_TWEAKS_ITEM_CLASS (klass);

  item_class->accepts = ide_tweaks_section_accepts;
}

static void
ide_tweaks_section_init (IdeTweaksSection *self)
{
}
