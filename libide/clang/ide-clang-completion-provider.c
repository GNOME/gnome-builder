/* ide-clang-completion-provider.h
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

#include "ide-buffer.h"
#include "ide-clang-completion-provider.h"
#include "ide-clang-service.h"
#include "ide-clang-translation-unit.h"
#include "ide-context.h"
#include "ide-debug.h"
#include "ide-file.h"

struct _IdeClangCompletionProviderClass
{
  GObjectClass parent_class;
};

struct _IdeClangCompletionProvider
{
  GObject parent_instance;
};

typedef struct
{
  GCancellable                *cancellable;
  GtkSourceCompletionProvider *provider;
  GtkSourceCompletionContext  *context;
  GFile                       *file;
} AddProposalsState;

static void completion_provider_iface_init (GtkSourceCompletionProviderIface *);

G_DEFINE_TYPE_EXTENDED (IdeClangCompletionProvider,
                        ide_clang_completion_provider,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                               completion_provider_iface_init))

static void
add_proposals_state_free (AddProposalsState *state)
{
  g_signal_handlers_disconnect_by_func (state->context,
                                        G_CALLBACK (g_cancellable_cancel),
                                        state->cancellable);

  g_clear_object (&state->provider);
  g_clear_object (&state->context);
  g_clear_object (&state->file);
  g_clear_object (&state->cancellable);
  g_free (state);
}

static void
ide_clang_completion_provider_class_init (IdeClangCompletionProviderClass *klass)
{
}

static void
ide_clang_completion_provider_init (IdeClangCompletionProvider *provider)
{
}

static gchar *
ide_clang_completion_provider_get_name (GtkSourceCompletionProvider *provider)
{
  return g_strdup (_("Clang"));
}

static void
ide_clang_completion_provider_complete_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeClangTranslationUnit *tu = (IdeClangTranslationUnit *)object;
  AddProposalsState *state = user_data;
  GError *error = NULL;
  GList *list;

  list = ide_clang_translation_unit_code_complete_finish (tu, result, &error);

  if (!list && error)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  if (!g_cancellable_is_cancelled (state->cancellable))
    gtk_source_completion_context_add_proposals (state->context, state->provider, list, TRUE);
  g_list_free_full (list, g_object_unref);
  add_proposals_state_free (state);
}

static void
ide_clang_completion_provider_tu_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeClangService *service = (IdeClangService *)object;
  g_autoptr(IdeClangTranslationUnit) tu = NULL;
  AddProposalsState *state = user_data;
  GError *error = NULL;
  GtkTextIter iter;

  g_assert (IDE_IS_CLANG_SERVICE (service));
  g_assert (state);
  g_assert (IDE_IS_CLANG_COMPLETION_PROVIDER (state->provider));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (state->context));
  g_assert (G_IS_FILE (state->file));

  tu = ide_clang_service_get_translation_unit_finish (service, result, &error);

  if (!tu)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      goto cleanup;
    }

  if (!gtk_source_completion_context_get_iter (state->context, &iter))
    goto cleanup;

  ide_clang_translation_unit_code_complete_async (tu,
                                                  state->file,
                                                  &iter,
                                                  NULL,
                                                  ide_clang_completion_provider_complete_cb,
                                                  state);

  return;

cleanup:
  if (!g_cancellable_is_cancelled (state->cancellable))
    gtk_source_completion_context_add_proposals (state->context, state->provider, NULL, TRUE);
  add_proposals_state_free (state);
}

static void
ide_clang_completion_provider_populate (GtkSourceCompletionProvider *provider,
                                        GtkSourceCompletionContext  *context)
{
  AddProposalsState *state;
  IdeClangService *service;
  GtkTextBuffer *buffer;
  IdeContext *icontext;
  GtkTextIter iter;
  IdeFile *file;

  if (!gtk_source_completion_context_get_iter (context, &iter))
    goto failure;

  buffer = gtk_text_iter_get_buffer (&iter);
  if (buffer == NULL)
    goto failure;

  g_assert (IDE_IS_BUFFER (buffer));

  file = ide_buffer_get_file (IDE_BUFFER (buffer));
  if (file == NULL)
    goto failure;

  g_assert (IDE_IS_FILE (file));

  icontext = ide_buffer_get_context (IDE_BUFFER (buffer));
  if (icontext == NULL)
    goto failure;

  g_assert (IDE_IS_CONTEXT (icontext));

  service = ide_context_get_service_typed (icontext, IDE_TYPE_CLANG_SERVICE);
  g_assert (IDE_IS_CLANG_SERVICE (service));

  state = g_new0 (AddProposalsState, 1);
  state->provider = g_object_ref (provider);
  state->context = g_object_ref (context);
  state->file = g_object_ref (ide_file_get_file (file));
  state->cancellable = g_cancellable_new ();

  g_signal_connect_swapped (context,
                            "cancelled",
                            G_CALLBACK (g_cancellable_cancel),
                            state->cancellable);

  ide_clang_service_get_translation_unit_async (service,
                                                file,
                                                0,
                                                NULL,
                                                ide_clang_completion_provider_tu_cb,
                                                state);

  return;

failure:
  gtk_source_completion_context_add_proposals (context, provider, NULL, TRUE);
}

static gboolean
ide_clang_completion_provider_get_start_iter (GtkSourceCompletionProvider *provider,
                                              GtkSourceCompletionContext  *context,
                                              GtkSourceCompletionProposal *proposal,
                                              GtkTextIter                 *iter)
{
  return FALSE;
}

static gboolean
ide_clang_completion_provider_activate_proposal (GtkSourceCompletionProvider *provider,
                                                 GtkSourceCompletionProposal *proposal,
                                                 GtkTextIter                 *iter)
{
  return FALSE;
}

static gint
ide_clang_completion_provider_get_interactive_delay (GtkSourceCompletionProvider *provider)
{
  return -1;
}

static void
completion_provider_iface_init (GtkSourceCompletionProviderIface *iface)
{
  iface->activate_proposal = ide_clang_completion_provider_activate_proposal;
  iface->get_interactive_delay = ide_clang_completion_provider_get_interactive_delay;
  iface->get_name = ide_clang_completion_provider_get_name;
  iface->get_start_iter = ide_clang_completion_provider_get_start_iter;
  iface->populate = ide_clang_completion_provider_populate;
}
