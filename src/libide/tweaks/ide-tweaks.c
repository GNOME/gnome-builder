/* ide-tweaks.c
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

#define G_LOG_DOMAIN "ide-tweaks"

#include "config.h"

#include <gtk/gtk.h>

#include "ide-tweaks.h"
#include "ide-tweaks-section.h"

struct _IdeTweaks
{
  IdeTweaksItem parent_instance;
};

G_DEFINE_FINAL_TYPE (IdeTweaks, ide_tweaks, IDE_TYPE_TWEAKS_ITEM)

enum {
  PROP_0,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static gboolean
ide_tweaks_accepts (IdeTweaksItem *item,
                    IdeTweaksItem *child)
{
  g_assert (IDE_IS_TWEAKS_ITEM (item));
  g_assert (IDE_IS_TWEAKS_ITEM (child));

  return IDE_IS_TWEAKS_SECTION (child);
}

static void
ide_tweaks_dispose (GObject *object)
{
  IdeTweaks *self = (IdeTweaks *)object;

  G_OBJECT_CLASS (ide_tweaks_parent_class)->dispose (object);
}

static void
ide_tweaks_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdeTweaks *self = IDE_TWEAKS (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  IdeTweaks *self = IDE_TWEAKS (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_class_init (IdeTweaksClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksItemClass *item_class = IDE_TWEAKS_ITEM_CLASS (klass);

  object_class->dispose = ide_tweaks_dispose;
  object_class->get_property = ide_tweaks_get_property;
  object_class->set_property = ide_tweaks_set_property;

  item_class->accepts = ide_tweaks_accepts;
}

static void
ide_tweaks_init (IdeTweaks *self)
{
}

IdeTweaks *
ide_tweaks_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS, NULL);
}

gboolean
ide_tweaks_load_from_file (IdeTweaks     *self,
                           GFile         *file,
                           GCancellable  *cancellable,
                           GError       **error)
{
  g_autoptr(GtkBuilder) builder = NULL;
  g_autofree char *contents = NULL;
  gsize length;

  g_return_val_if_fail (IDE_IS_TWEAKS (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (!g_file_load_contents (file, cancellable, &contents, &length, NULL, error))
    return FALSE;

  builder = gtk_builder_new ();

  return gtk_builder_extend_with_template (builder, G_OBJECT (self), IDE_TYPE_TWEAKS, contents, length, error);
}
