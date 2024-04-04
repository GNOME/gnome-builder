/*
 * manuals-importer.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "manuals-importer.h"

G_DEFINE_ABSTRACT_TYPE (ManualsImporter, manuals_importer, G_TYPE_OBJECT)

static void
manuals_importer_class_init (ManualsImporterClass *klass)
{
}

static void
manuals_importer_init (ManualsImporter *self)
{
}

DexFuture *
manuals_importer_import (ManualsImporter   *self,
                         ManualsRepository *repository,
                         ManualsProgress   *progress)
{
  g_return_val_if_fail (MANUALS_IS_IMPORTER (self), NULL);
  g_return_val_if_fail (MANUALS_IS_REPOSITORY (repository), NULL);
  g_return_val_if_fail (MANUALS_IS_PROGRESS (progress), NULL);

  return MANUALS_IMPORTER_GET_CLASS (self)->import (self, repository, progress);
}
