/* ide-xml-analysis.c
 *
 * Copyright 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include "ide-xml-analysis.h"

G_DEFINE_BOXED_TYPE (IdeXmlAnalysis, ide_xml_analysis, ide_xml_analysis_ref, ide_xml_analysis_unref)

gint64
ide_xml_analysis_get_sequence (IdeXmlAnalysis *self)
{
  g_return_val_if_fail (self, -1);

  return self->sequence;
}

/**
 * ide_xml_analysis_get_diagnostics:
 * @self: an #IdeXmlAnalysis.
 *
 * Returns: (nullable) (transfer none): The #IdeDiagnostics contained by the analysis.
 *
 */
IdeDiagnostics *
ide_xml_analysis_get_diagnostics (IdeXmlAnalysis *self)
{
  g_return_val_if_fail (self, NULL);

  return self->diagnostics;
}

/**
 * ide_xml_analysis_get_diagnostics:
 * @self: an #IdeXmlAnalysis.
 *
 * Returns: (nullable) (transfer none): The #IdeXmlSymbolNode root node contained by the analysis.
 *
 */
IdeXmlSymbolNode *
ide_xml_analysis_get_root_node (IdeXmlAnalysis *self)
{
  g_return_val_if_fail (self, NULL);

  return self->root_node;
}

/**
 * ide_xml_analysis_get_schemas:
 * @self: a #GPtrArray.
 *
 * Returns: (nullable) (transfer none): The schemas entries #GPtrArray contained by the analysis.
 *
 */
GPtrArray *
ide_xml_analysis_get_schemas (IdeXmlAnalysis *self)
{
  g_return_val_if_fail (self, NULL);

  return self->schemas;
}

void
ide_xml_analysis_set_diagnostics (IdeXmlAnalysis *self,
                                  IdeDiagnostics *diagnostics)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (diagnostics != NULL);

  g_set_object (&self->diagnostics, diagnostics);
}

void
ide_xml_analysis_set_root_node (IdeXmlAnalysis   *self,
                                IdeXmlSymbolNode *root_node)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (root_node != NULL);

  g_set_object (&self->root_node, root_node);
}

void
ide_xml_analysis_set_schemas (IdeXmlAnalysis *self,
                              GPtrArray      *schemas)
{
  g_return_if_fail (self != NULL);

  if (self->schemas != schemas)
    {
      g_clear_pointer (&self->schemas, g_ptr_array_unref);
      if (schemas != NULL)
        self->schemas = g_ptr_array_ref (schemas);
    }
}

void
ide_xml_analysis_set_sequence (IdeXmlAnalysis   *self,
                               gint64            sequence)
{
  g_return_if_fail (self != NULL);

  self->sequence = sequence;
}

IdeXmlAnalysis *
ide_xml_analysis_new (gint64 sequence)
{
  IdeXmlAnalysis *self;

  self = g_slice_new0 (IdeXmlAnalysis);
  self->ref_count = 1;
  self->sequence = sequence;

  return self;
}

static void
ide_xml_analysis_free (IdeXmlAnalysis *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  g_clear_object (&self->root_node);
  g_clear_object (&self->diagnostics);
  g_slice_free (IdeXmlAnalysis, self);
}

IdeXmlAnalysis *
ide_xml_analysis_ref (IdeXmlAnalysis *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_xml_analysis_unref (IdeXmlAnalysis *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_xml_analysis_free (self);
}
