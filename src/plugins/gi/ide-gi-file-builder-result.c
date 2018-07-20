/* ide-gi-file-builder-result.c
 *
 * Copyright © 2017 Sebastien Lafargue <slafargue@gnome.org>
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
 */

#include <dazzle.h>

#include "ide-gi-file-builder-result.h"

G_DEFINE_BOXED_TYPE (IdeGiFileBuilderResult, ide_gi_file_builder_result, ide_gi_file_builder_result_ref, ide_gi_file_builder_result_unref)

IdeGiFileBuilderResult *
ide_gi_file_builder_result_new (GByteArray            *ns_ba,
                                IdeGiRadixTreeBuilder *ro_tree,
                                GArray                *global_index,
                                const gchar           *ns,
                                const gchar           *symbol_prefixes,
                                const gchar           *identifier_prefixes)
{
  IdeGiFileBuilderResult *self;

  self = g_slice_new0 (IdeGiFileBuilderResult);
  self->ref_count = 1;

  self->ns_ba = g_byte_array_ref (ns_ba);
  self->ro_tree = g_object_ref (ro_tree);
  self->global_index = g_array_ref (global_index);

  self->ns = g_strdup (ns);
  self->symbol_prefixes = g_strdup (symbol_prefixes);
  self->identifier_prefixes = g_strdup (identifier_prefixes);

  return self;
}

static void
ide_gi_file_builder_result_free (IdeGiFileBuilderResult *self)
{
  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  dzl_clear_pointer (&self->ns_ba, g_byte_array_unref);
  dzl_clear_pointer (&self->global_index, g_array_unref);
  g_clear_object (&self->ro_tree);

  dzl_clear_pointer (&self->ns, g_free);
  dzl_clear_pointer (&self->symbol_prefixes, g_free);
  dzl_clear_pointer (&self->identifier_prefixes, g_free);

  g_clear_object (&self->ro_tree);

  g_slice_free (IdeGiFileBuilderResult, self);
}

IdeGiFileBuilderResult *
ide_gi_file_builder_result_ref (IdeGiFileBuilderResult *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_file_builder_result_unref (IdeGiFileBuilderResult *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_file_builder_result_free (self);
}
