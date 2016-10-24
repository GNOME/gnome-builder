/* ide-diagnostician.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-diagnostician"

#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-debug.h"
#include "ide-internal.h"

#include "diagnostics/ide-diagnostic-provider.h"
#include "diagnostics/ide-diagnostician.h"
#include "diagnostics/ide-diagnostics.h"
#include "plugins/ide-extension-set-adapter.h"
#include "files/ide-file.h"

struct _IdeDiagnostician
{
  IdeObject               parent_instance;

  GtkSourceLanguage      *language;
  IdeExtensionSetAdapter *extensions;
};

typedef struct
{
  /*
   * Weak, non-owned pointers.
   * Only valid during dispatch of diagnose call.
   */
  IdeFile        *file;
  GCancellable   *cancellable;
  GTask          *task;

  /*
   * Owned by diagnose state.
   */
  IdeDiagnostics *diagnostics;
  guint           total;
  guint           active;
} DiagnoseState;

G_DEFINE_TYPE (IdeDiagnostician, ide_diagnostician, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_LANGUAGE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

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

  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));
  g_assert (G_IS_TASK (task));

  state = g_task_get_task_data (task);

  g_assert (state != NULL);
  g_assert (state->task == task);

  state->active--;

  ret = ide_diagnostic_provider_diagnose_finish (provider, result, &error);

  if (ret == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);
      goto maybe_complete;
    }

  ide_diagnostics_merge (state->diagnostics, ret);
  ide_diagnostics_unref (ret);

maybe_complete:
  IDE_TRACE_MSG ("%d of %d diagnostic providers active",
                 state->active, state->total);

  if (state->active == 0)
    g_task_return_pointer (task,
                           g_steal_pointer (&state->diagnostics),
                           (GDestroyNotify)ide_diagnostics_unref);
}

static void
ide_diagnostician_run_diagnose_cb (IdeExtensionSetAdapter *adapter,
                                   PeasPluginInfo         *plugin_info,
                                   PeasExtension          *exten,
                                   gpointer                user_data)
{
  IdeDiagnosticProvider *provider = (IdeDiagnosticProvider *)exten;
  DiagnoseState *state = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (IDE_IS_DIAGNOSTIC_PROVIDER (provider));
  g_assert (state != NULL);

  ide_diagnostic_provider_diagnose_async (provider,
                                          state->file,
                                          state->cancellable,
                                          diagnose_cb,
                                          g_object_ref (state->task));
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
  guint count;

  g_return_if_fail (IDE_IS_DIAGNOSTICIAN (self));
  g_return_if_fail (IDE_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_assert (self->extensions != NULL);

  task = g_task_new (self, cancellable, callback, user_data);

  count = ide_extension_set_adapter_get_n_extensions (self->extensions);

  if (count == 0)
    {
      g_task_return_pointer (task,
                             ide_diagnostics_new (NULL),
                             (GDestroyNotify)ide_diagnostics_unref);
      return;
    }

  state = g_slice_new0 (DiagnoseState);
  state->file = file;
  state->cancellable = cancellable;
  state->task = task;
  state->active = count;
  state->total = count;
  state->diagnostics = ide_diagnostics_new (NULL);

  g_task_set_task_data (task, state, diagnose_state_free);

  ide_extension_set_adapter_foreach (self->extensions,
                                     ide_diagnostician_run_diagnose_cb,
                                     state);
}

IdeDiagnostics *
ide_diagnostician_diagnose_finish (IdeDiagnostician  *self,
                                   GAsyncResult      *result,
                                   GError           **error)
{
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_diagnostician_constructed (GObject *object)
{
  IdeDiagnostician *self = (IdeDiagnostician *)object;
  const gchar *lang_id = NULL;
  IdeContext *context;

  G_OBJECT_CLASS (ide_diagnostician_parent_class)->constructed (object);

  if (self->language != NULL)
    lang_id = gtk_source_language_get_id (self->language);

  context = ide_object_get_context (IDE_OBJECT (object));

  g_assert (IDE_IS_CONTEXT (context));

  self->extensions = ide_extension_set_adapter_new (context,
                                                    peas_engine_get_default (),
                                                    IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                                    "Diagnostic-Provider-Languages",
                                                    lang_id);
}

static void
ide_diagnostician_dispose (GObject *object)
{
  IdeDiagnostician *self = (IdeDiagnostician *)object;

  g_clear_object (&self->extensions);

  G_OBJECT_CLASS (ide_diagnostician_parent_class)->dispose (object);
}

static void
ide_diagnostician_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeDiagnostician *self = IDE_DIAGNOSTICIAN(object);

  switch (prop_id)
    {
    case PROP_LANGUAGE:
      g_value_set_object (value, ide_diagnostician_get_language (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_diagnostician_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeDiagnostician *self = IDE_DIAGNOSTICIAN(object);

  switch (prop_id)
    {
    case PROP_LANGUAGE:
      ide_diagnostician_set_language (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_diagnostician_class_init (IdeDiagnosticianClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_diagnostician_constructed;
  object_class->dispose = ide_diagnostician_dispose;
  object_class->get_property = ide_diagnostician_get_property;
  object_class->set_property = ide_diagnostician_set_property;

  properties [PROP_LANGUAGE] =
    g_param_spec_object ("language",
                         "Language",
                         "Language",
                         GTK_SOURCE_TYPE_LANGUAGE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_diagnostician_init (IdeDiagnostician *self)
{
}

/**
 * ide_diagnostician_get_language:
 *
 * Gets the #IdeDiagnostician:language property.
 *
 * Returns: (transfer none): A #GtkSourceLanguage.
 */
GtkSourceLanguage *
ide_diagnostician_get_language (IdeDiagnostician *self)
{
  g_return_val_if_fail (IDE_IS_DIAGNOSTICIAN (self), NULL);

  return self->language;
}

void
ide_diagnostician_set_language (IdeDiagnostician  *self,
                                GtkSourceLanguage *language)
{
  g_return_if_fail (IDE_IS_DIAGNOSTICIAN (self));
  g_return_if_fail (!language || GTK_SOURCE_IS_LANGUAGE (language));

  if (g_set_object (&self->language, language))
    {
      if (self->extensions != NULL)
        {
          const gchar *lang_id = NULL;

          if (language != NULL)
            lang_id = gtk_source_language_get_id (language);

          ide_extension_set_adapter_set_value (self->extensions, lang_id);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LANGUAGE]);
    }
}
