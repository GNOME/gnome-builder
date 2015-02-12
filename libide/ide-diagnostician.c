/* ide-diagnostician.c
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

#include "ide-diagnostician.h"

typedef struct
{
  GPtrArray *providers;
} IdeDiagnosticianPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeDiagnostician, ide_diagnostician,
                            IDE_TYPE_OBJECT)

enum {
  PROP_0,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

void
_ide_diagnostician_add_provider (IdeDiagnostician      *diagnostician,
                                 IdeDiagnosticProvider *provider)
{

}

void
_ide_diagnostician_remove_provider (IdeDiagnostician      *diagnostician,
                                    IdeDiagnosticProvider *provider)
{

}

static void
ide_diagnostician_dispose (GObject *object)
{
  IdeDiagnostician *self = (IdeDiagnostician *)object;
  IdeDiagnosticianPrivate *priv = ide_diagnostician_get_instance_private (self);

  g_clear_pointer (&priv->providers, g_ptr_array_unref);

  G_OBJECT_CLASS (ide_diagnostician_parent_class)->dispose (object);
}

static void
ide_diagnostician_class_init (IdeDiagnosticianClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_diagnostician_dispose;
}

static void
ide_diagnostician_init (IdeDiagnostician *self)
{
  IdeDiagnosticianPrivate *priv = ide_diagnostician_get_instance_private (self);

  priv->providers = g_ptr_array_new_with_free_func (g_object_unref);
}
