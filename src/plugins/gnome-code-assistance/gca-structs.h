/* gca-structs.h
 *
 * Copyright 2014-2019 Christian Hergert <christian@hergert.me>
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

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  GCA_SEVERITY_NONE,
  GCA_SEVERITY_INFO,
  GCA_SEVERITY_WARNING,
  GCA_SEVERITY_DEPRECATED,
  GCA_SEVERITY_ERROR,
  GCA_SEVERITY_FATAL,
} GcaSeverity;

typedef struct
{
  guint64 line;
  guint64 column;
} GcaSourceLocation;

typedef struct
{
  gint64            file;
  GcaSourceLocation begin;
  GcaSourceLocation end;
} GcaSourceRange;

typedef struct
{
  GcaSourceRange  range;
  gchar          *value;
} GcaFixit;

typedef struct
{
  GcaSeverity  severity;
  GArray      *fixits;
  GArray      *locations;
  gchar       *message;
} GcaDiagnostic;

GArray *gca_diagnostics_from_variant (GVariant *variant);

G_END_DECLS
