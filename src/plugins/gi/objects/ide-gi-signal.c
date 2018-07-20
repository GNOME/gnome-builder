/* ide-gi-signal.c
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#include "./../ide-gi-utils.h"

#include "ide-gi-signal.h"

G_DEFINE_BOXED_TYPE (IdeGiSignal,
                     ide_gi_signal,
                     ide_gi_signal_ref,
                     ide_gi_signal_unref)

void
ide_gi_signal_dump (IdeGiSignal *self,
                    guint        depth)
{
  guint n_parameters;
  IdeGiSignalWhen run_when;

  g_return_if_fail (self != NULL);

  run_when = ide_gi_signal_get_run_when (self);

  ide_gi_base_dump ((IdeGiBase *)self);

  g_print ("no recurse:%d\n", ide_gi_signal_is_no_recurse (self));
  g_print ("detailed:%d\n", ide_gi_signal_is_detailed (self));
  g_print ("no action:%d\n", ide_gi_signal_is_action (self));
  g_print ("no hooks:%d\n", ide_gi_signal_is_no_hooks (self));
  g_print ("class closure:%d\n", ide_gi_signal_has_class_closure (self));
  g_print ("true stops emit:%d\n", ide_gi_signal_is_true_stops_emit (self));

  g_print ("run when:%s\n", ide_gi_utils_signal_when_to_string (run_when));
  g_print ("vfunc:%s\n", ide_gi_signal_get_vfunc (self));

  n_parameters = ide_gi_signal_get_n_parameters (self);
  g_print ("n parameters:%d\n", n_parameters);

  if (depth > 0)
    {
      for (guint i = 0; i < n_parameters; i++)
        ide_gi_parameter_dump (ide_gi_signal_get_parameter (self, i), depth - 1);
    }
}

IdeGiSignalWhen
ide_gi_signal_get_run_when (IdeGiSignal *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiSignalBlob *)self->common_blob)->run_when;
}

gboolean
ide_gi_signal_is_no_recurse (IdeGiSignal *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiSignalBlob *)self->common_blob)->no_recurse;
}

gboolean
ide_gi_signal_is_detailed (IdeGiSignal *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiSignalBlob *)self->common_blob)->detailed;
}

gboolean
ide_gi_signal_is_action (IdeGiSignal *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiSignalBlob *)self->common_blob)->action;
}

gboolean
ide_gi_signal_is_no_hooks (IdeGiSignal *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiSignalBlob *)self->common_blob)->no_hooks;
}

gboolean
ide_gi_signal_has_class_closure (IdeGiSignal *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiSignalBlob *)self->common_blob)->has_class_closure;
}

gboolean
ide_gi_signal_is_true_stops_emit (IdeGiSignal *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return ((IdeGiSignalBlob *)self->common_blob)->true_stops_emit;
}

const gchar *
ide_gi_signal_get_vfunc (IdeGiSignal *self)
{
  guint32 offset;

  g_return_val_if_fail (self != NULL, FALSE);

  offset = ((IdeGiSignalBlob *)self->common_blob)->function;
  return ide_gi_namespace_get_string (self->ns, offset);
}

guint16
ide_gi_signal_get_n_parameters (IdeGiSignal *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return ((IdeGiSignalBlob *)self->common_blob)->n_parameters;
}

IdeGiParameter *
ide_gi_signal_get_parameter (IdeGiSignal *self,
                             guint16      nth)
{
  IdeGiParameter *parameter = NULL;
  guint16 n_parameters;

  g_return_val_if_fail (self != NULL, NULL);

  if (nth + 1 > (n_parameters = ide_gi_signal_get_n_parameters (self)))
    g_warning ("Parameter %d is out of bounds (nb parameters = %d)", nth, n_parameters);
  else
    {
      guint16 offset = ((IdeGiSignalBlob *)self->common_blob)->parameters + nth;
      parameter = (IdeGiParameter *)ide_gi_parameter_new (self->ns, IDE_GI_BLOB_TYPE_PARAMETER, offset);
    }

  return parameter;
}

IdeGiParameter *
ide_gi_signal_lookup_parameter (IdeGiSignal *self,
                                const gchar *name)
{
  guint16 n_parameters;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (!dzl_str_empty0 (name), NULL);

  n_parameters = ide_gi_signal_get_n_parameters (self);
  for (guint i = 0; i < n_parameters; i++)
    {
      const gchar *parameter_name;
      g_autoptr(IdeGiParameter) parameter = ide_gi_signal_get_parameter (self, i);

      if (parameter == NULL)
        continue;

      parameter_name = ide_gi_base_get_name ((IdeGiBase *)parameter);
      if (dzl_str_equal0 (parameter_name, name))
        return g_steal_pointer (&parameter);
    }

  return NULL;
}

IdeGiParameter *
ide_gi_signal_get_return_value (IdeGiSignal *self)
{
  guint16 offset;

  g_return_val_if_fail (self != NULL, NULL);

  offset = ((IdeGiSignalBlob *)self->common_blob)->return_value;
  return (IdeGiParameter *)ide_gi_parameter_new (self->ns, IDE_GI_BLOB_TYPE_PARAMETER, offset);
}

IdeGiBase *
ide_gi_signal_new (IdeGiNamespace *ns,
                   IdeGiBlobType   type,
                   gint32          offset)
{
  IdeGiSignal *self;
  guint8 *table;
  gsize type_size;

  g_return_val_if_fail (ns != NULL, NULL);
  g_return_val_if_fail (offset > -1, NULL);
  g_return_val_if_fail (type != IDE_GI_BLOB_TYPE_UNKNOW, NULL);

  self = g_slice_new0 (IdeGiSignal);
  self->ref_count = 1;

  self->ns = ide_gi_namespace_ref (ns);
  self->type = type;
  self->offset = offset;

  table = _ide_gi_namespace_get_table_from_type (ns, type);
  type_size = _ide_gi_namespace_get_blob_size_from_type (ns, type);

NO_CAST_ALIGN_PUSH
  self->common_blob = (IdeGiCommonBlob *)(table + offset * type_size);
NO_CAST_ALIGN_POP

  return (IdeGiBase *)self;
}

void
ide_gi_signal_free (IdeGiBase *base)
{
  IdeGiSignal *self = (IdeGiSignal *)base;

  g_assert (self);
  g_assert_cmpint (self->ref_count, ==, 0);

  ide_gi_namespace_unref (self->ns);
  g_slice_free (IdeGiSignal, self);
}

IdeGiSignal *
ide_gi_signal_ref (IdeGiSignal *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_gi_signal_unref (IdeGiSignal *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    ide_gi_signal_free ((IdeGiBase *)self);
}
