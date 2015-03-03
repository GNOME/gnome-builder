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

#include <glib/gi18n.h>

#include "ide-diagnostic-provider.h"
#include "ide-diagnostician.h"
#include "ide-diagnostics.h"
#include "ide-file.h"
#include "ide-internal.h"

struct _IdeDiagnostician
{
  IdeObject  parent_instance;

  GPtrArray *providers;
};

typedef struct
{
  IdeDiagnostics *diagnostics;
  guint           total;
  guint           active;
} DiagnoseState;

G_DEFINE_TYPE (IdeDiagnostician, ide_diagnostician, IDE_TYPE_OBJECT)

static void
diagnose_state_free (gpointer data)
{
  DiagnoseState *state = data;

  if (state)
    {
      g_clear_pointer (&state->diagnostics, ide_diagnostics_unref);
      g_slice_free (DiagnoseState, state);
    }
}

void
_ide_diagnostician_add_provider (IdeDiagnostician      *self,
                                 IdeDiagnosticProvider *provider)
{
  g_return_if_fail (IDE_IS_DIAGNOSTICIAN (self));
  g_return_if_fail (IDE_IS_DIAGNOSTIC_PROVIDER (provider));

  g_ptr_array_add (self->providers, g_object_ref (provider));
}

void
_ide_diagnostician_remove_provider (IdeDiagnostician      *self,
                                    IdeDiagnosticProvider *provider)
{
  g_return_if_fail (IDE_IS_DIAGNOSTICIAN (self));
  g_return_if_fail (IDE_IS_DIAGNOSTIC_PROVIDER (provider));

  g_ptr_array_remove (self->providers, provider);
}

static void
diagnose_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  IdeDiagnosticProvider *provider = (IdeDiagnosticProvider *)object;
  IdeDiagnostics *ret;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  DiagnoseState *state;

  g_return_if_fail (IDE_IS_DIAGNOSTIC_PROVIDER (provider));
  g_return_if_fail (G_IS_TASK (task));

  state = g_task_get_task_data (task);

  state->active--;

  ret = ide_diagnostic_provider_diagnose_finish (provider, result, &error);

  if (!ret)
    {
      g_info ("%s", error->message);
      goto maybe_complete;
    }

  ide_diagnostics_merge (state->diagnostics, ret);
  ide_diagnostics_unref (ret);

maybe_complete:
  if (state->total == 1 && error)
    g_task_return_error (task, g_error_copy (error));
  else if (!state->active)
    g_task_return_pointer (task,
                           ide_diagnostics_ref (state->diagnostics),
                           (GDestroyNotify)ide_diagnostics_unref);
}

void
ide_diagnostician_diagnose_async (IdeDiagnostician    *self,
                                  IdeFile             *file,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  DiagnoseState *state;
  g_autoptr(GTask) task = NULL;
  gsize i;

  g_return_if_fail (IDE_IS_DIAGNOSTICIAN (self));
  g_return_if_fail (IDE_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  if (!self->providers->len)
    {
      g_task_return_pointer (task,
                             _ide_diagnostics_new (NULL),
                             (GDestroyNotify)g_ptr_array_unref);
      return;
    }

  state = g_slice_new0 (DiagnoseState);
  state->active = self->providers->len;
  state->total = self->providers->len;
  state->diagnostics = _ide_diagnostics_new (NULL);

  g_task_set_task_data (task, state, diagnose_state_free);

  for (i = 0; i < self->providers->len; i++)
    {
      IdeDiagnosticProvider *provider;

      provider = g_ptr_array_index (self->providers, i);
      ide_diagnostic_provider_diagnose_async (provider,
                                              file,
                                              cancellable,
                                              diagnose_cb,
                                              g_object_ref (task));
    }
}

IdeDiagnostics *
ide_diagnostician_diagnose_finish (IdeDiagnostician  *self,
                                   GAsyncResult      *result,
                                   GError           **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_diagnostician_dispose (GObject *object)
{
  IdeDiagnostician *self = (IdeDiagnostician *)object;

  g_clear_pointer (&self->providers, g_ptr_array_unref);

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
  self->providers = g_ptr_array_new_with_free_func (g_object_unref);
}
