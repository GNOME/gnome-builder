/* ide-c-indenter.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ide-c-indenter.h"

typedef struct
{
  void *foo;
} IdeCIndenterPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeCIndenter, ide_c_indenter, IDE_TYPE_INDENTER)

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
ide_c_indenter_finalize (GObject *object)
{
  IdeCIndenter *self = (IdeCIndenter *)object;
  IdeCIndenterPrivate *priv = ide_c_indenter_get_instance_private (self);

  G_OBJECT_CLASS (ide_c_indenter_parent_class)->finalize (object);
}

static void
ide_c_indenter_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeCIndenter *self = IDE_C_INDENTER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_c_indenter_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeCIndenter *self = IDE_C_INDENTER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_c_indenter_class_init (IdeCIndenterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_c_indenter_finalize;
  object_class->get_property = ide_c_indenter_get_property;
  object_class->set_property = ide_c_indenter_set_property;
}

static void
ide_c_indenter_init (IdeCIndenter *self)
{
}
