/* ide-completion-provider.h
 *
 * Copyright © 2018 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <gtk/gtk.h>

#include "ide-context.h"
#include "ide-version-macros.h"

#include "completion/ide-completion-compat.h"
#include "completion/ide-completion-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION_PROVIDER (ide_completion_provider_get_type ())

IDE_AVAILABLE_IN_3_30
G_DECLARE_INTERFACE (IdeCompletionProvider, ide_completion_provider, IDE, COMPLETION_PROVIDER, GObject)

struct _IdeCompletionProviderInterface
{
  GTypeInterface parent;

  void        (*load)              (IdeCompletionProvider  *self,
                                    IdeContext             *context);
  GIcon      *(*get_icon)          (IdeCompletionProvider  *self);
  gint        (*get_priority)      (IdeCompletionProvider  *self);
  gchar      *(*get_title)         (IdeCompletionProvider  *self);
  void        (*populate_async)    (IdeCompletionProvider  *self,
                                    GCancellable           *cancellable,
                                    GListModel            **results,
                                    GAsyncReadyCallback     callback,
                                    gpointer                user_data);
  GListModel *(*populate_finish)   (IdeCompletionProvider  *self,
                                    GAsyncResult           *result,
                                    GError                **error);
  void        (*activate_proposal) (IdeCompletionProvider *self,
                                    IdeCompletionContext  *context,
                                    IdeCompletionProposal *proposal,
                                    const GdkEventKey     *key);
  gboolean    (*refilter)          (IdeCompletionProvider *self,
                                    IdeCompletionContext  *context,
                                    GListModel            *proposals);
  gboolean    (*is_trigger)        (IdeCompletionProvider *self,
                                    const GtkTextIter     *iter,
                                    gunichar               ch);
  gboolean    (*key_activates)     (IdeCompletionProvider *self,
                                    IdeCompletionProposal *proposal,
                                    const GdkEventKey     *key);
};

IDE_AVAILABLE_IN_3_30
void        ide_completion_provider_load             (IdeCompletionProvider  *self,
                                                      IdeContext             *context);
IDE_AVAILABLE_IN_3_30
GIcon      *ide_completion_provider_get_icon         (IdeCompletionProvider  *self);
IDE_AVAILABLE_IN_3_30
gint        ide_completion_provider_get_priority     (IdeCompletionProvider  *self);
IDE_AVAILABLE_IN_3_30
gchar      *ide_completion_provider_get_title        (IdeCompletionProvider  *self);
IDE_AVAILABLE_IN_3_30
void        ide_completion_provider_populate_async   (IdeCompletionProvider  *self,
                                                      GCancellable           *cancellable,
                                                      GListModel            **results,
                                                      GAsyncReadyCallback     callback,
                                                      gpointer                user_data);
IDE_AVAILABLE_IN_3_30
GListModel *ide_completion_provider_populate_finish  (IdeCompletionProvider  *self,
                                                      GAsyncResult           *result,
                                                      GError                **error);
IDE_AVAILABLE_IN_3_30
void        ide_completion_provider_activate_poposal (IdeCompletionProvider  *self,
                                                      IdeCompletionContext   *context,
                                                      IdeCompletionProposal  *proposal,
                                                      const GdkEventKey      *key);
IDE_AVAILABLE_IN_3_30
gboolean    ide_completion_provider_refilter         (IdeCompletionProvider  *self,
                                                      IdeCompletionContext   *context,
                                                      GListModel             *proposals);
IDE_AVAILABLE_IN_3_30
gboolean    ide_completion_provider_is_trigger       (IdeCompletionProvider  *self,
                                                      const GtkTextIter      *iter,
                                                      gunichar                ch);
IDE_AVAILABLE_IN_3_30
gboolean    ide_completion_provider_key_activates    (IdeCompletionProvider  *self,
                                                      IdeCompletionProposal  *proposal,
                                                      const GdkEventKey      *key);

G_END_DECLS
