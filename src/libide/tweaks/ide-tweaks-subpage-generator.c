/* ide-tweaks-subpage-generator.c
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

#define G_LOG_DOMAIN "ide-tweaks-subpage-generator"

#include "config.h"

#include "ide-tweaks-subpage-generator.h"

typedef struct
{
  guint populated : 1;
} IdeTweaksSubpageGeneratorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeTweaksSubpageGenerator, ide_tweaks_subpage_generator, IDE_TYPE_TWEAKS_ITEM)

enum {
  POPULATE,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
ide_tweaks_subpage_generator_class_init (IdeTweaksSubpageGeneratorClass *klass)
{
  signals [POPULATE] =
    g_signal_new ("populate",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeTweaksSubpageGeneratorClass, populate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void
ide_tweaks_subpage_generator_init (IdeTweaksSubpageGenerator *self)
{
}

void
ide_tweaks_subpage_generator_populate (IdeTweaksSubpageGenerator *self)
{
  IdeTweaksSubpageGeneratorPrivate *priv = ide_tweaks_subpage_generator_get_instance_private (self);

  g_return_if_fail (IDE_IS_TWEAKS_SUBPAGE_GENERATOR (self));

  if (!priv->populated)
    {
      priv->populated = TRUE;
      g_signal_emit (self, signals [POPULATE], 0);
    }
}
