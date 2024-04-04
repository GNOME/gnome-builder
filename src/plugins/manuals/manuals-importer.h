/*
 * manuals-importer.h
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

#pragma once

#include <libdex.h>

#include "manuals-repository.h"
#include "manuals-progress.h"

G_BEGIN_DECLS

#define MANUALS_TYPE_IMPORTER (manuals_importer_get_type())

G_DECLARE_DERIVABLE_TYPE (ManualsImporter, manuals_importer, MANUALS, IMPORTER, GObject)

struct _ManualsImporterClass
{
  GObjectClass parent_class;

  DexFuture *(*import) (ManualsImporter   *self,
                        ManualsRepository *repository,
                        ManualsProgress   *progress);
};

DexFuture *manuals_importer_import (ManualsImporter   *self,
                                    ManualsRepository *repository,
                                    ManualsProgress   *progress);

G_END_DECLS
