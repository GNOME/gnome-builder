/* gca-structs.c
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

#include "gca-structs.h"

static void
gca_diagnostic_destroy (gpointer data)
{
  GcaDiagnostic *diag = data;

  if (diag)
    {
      g_array_unref (diag->fixits);
      g_array_unref (diag->locations);
      g_free (diag->message);
    }
}

static void
gca_fixit_destroy (gpointer data)
{
  GcaFixit *fixit = data;

  if (fixit)
    g_free (fixit->value);
}

GArray *
gca_diagnostics_from_variant (GVariant *variant)
{
  GVariantIter iter;
  GArray *ret;
  GVariantIter *b;
  GVariantIter *c;
  gchar *d;
  guint a;

  g_return_val_if_fail (variant, NULL);

  ret = g_array_new (FALSE, FALSE, sizeof (GcaDiagnostic));

  g_array_set_clear_func (ret, gca_diagnostic_destroy);

  g_variant_iter_init (&iter, variant);

  while (g_variant_iter_loop (&iter, "(ua((x(xx)(xx))s)a(x(xx)(xx))s)",
                              &a, &b, &c, &d))
    {
      GcaDiagnostic diag = { 0 };
      gint64 x1, x2, x3, x4, x5;
      gchar *e;

      diag.severity = a;
      diag.fixits = g_array_new (FALSE, FALSE, sizeof (GcaFixit));
      diag.locations = g_array_new (FALSE, FALSE, sizeof (GcaSourceRange));
      diag.message = g_strdup (d);

      g_array_set_clear_func (diag.fixits, gca_fixit_destroy);

      while (g_variant_iter_next (b, "((x(xx)(xx))s)", &x1, &x2, &x3, &x4, &x5, &e))
        {
          GcaFixit fixit = {{ 0 }};

          fixit.range.file = x1;
          fixit.range.begin.line = x2 - 1;
          fixit.range.begin.column = x3 - 1;
          fixit.range.end.line = x4 - 1;
          fixit.range.end.column = x5 - 1;
          fixit.value = g_strdup (e);

          g_array_append_val (diag.fixits, fixit);
        }

      while (g_variant_iter_next (c, "(x(xx)(xx))", &x1, &x2, &x3, &x4, &x5))
        {
          GcaSourceRange range = { 0 };

          range.file = x1;
          range.begin.line = x2 - 1;
          range.begin.column = x3 - 1;
          range.end.line = x4 - 1;
          range.end.column = x5 - 1;

          g_array_append_val (diag.locations, range);
        }

      g_array_append_val (ret, diag);
    }

  return ret;
}
