/*
 * manuals-devhelp-importer.h
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

#include "manuals-importer.h"

G_BEGIN_DECLS

#define MANUALS_TYPE_DEVHELP_IMPORTER (manuals_devhelp_importer_get_type())

G_DECLARE_FINAL_TYPE (ManualsDevhelpImporter, manuals_devhelp_importer, MANUALS, DEVHELP_IMPORTER, ManualsImporter)

ManualsDevhelpImporter *manuals_devhelp_importer_new           (void);
guint                   manuals_devhelp_importer_get_size      (ManualsDevhelpImporter *self);
void                    manuals_devhelp_importer_add_directory (ManualsDevhelpImporter *self,
                                                                const char             *directory,
                                                                gint64                  sdk_id);
void                    manuals_devhelp_importer_set_sdk_id    (ManualsDevhelpImporter *self,
                                                                gint64                  sdk_id);

G_END_DECLS
