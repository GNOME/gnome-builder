/* gbp-ls-model.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

G_BEGIN_DECLS

#define GBP_TYPE_LS_MODEL (gbp_ls_model_get_type())

G_DECLARE_FINAL_TYPE (GbpLsModel, gbp_ls_model, GBP, LS_MODEL, GObject)

typedef enum {
  GBP_LS_MODEL_COLUMN_GICON,
  GBP_LS_MODEL_COLUMN_NAME,
  GBP_LS_MODEL_COLUMN_SIZE,
  GBP_LS_MODEL_COLUMN_MODIFIED,
  GBP_LS_MODEL_COLUMN_FILE,
  GBP_LS_MODEL_COLUMN_TYPE,
  GBP_LS_MODEL_N_COLUMNS
} GbpLsModelColumn;

GbpLsModel *gbp_ls_model_new           (GFile      *directory);
GFile      *gbp_ls_model_get_directory (GbpLsModel *self);

G_END_DECLS
