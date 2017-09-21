/* ide-completion-provider.h
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

#ifndef IDE_COMPLETION_PROVIDER_H
#define IDE_COMPLETION_PROVIDER_H

#include <gtksourceview/gtksource.h>

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION_PROVIDER         (ide_completion_provider_get_type())
#define IDE_COMPLETION_PROVIDER(o)           (G_TYPE_CHECK_INSTANCE_CAST((o),    IDE_TYPE_COMPLETION_PROVIDER, IdeCompletionProvider))
#define IDE_IS_COMPLETION_PROVIDER(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o),    IDE_TYPE_COMPLETION_PROVIDER))
#define IDE_COMPLETION_PROVIDER_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE((o), IDE_TYPE_COMPLETION_PROVIDER, IdeCompletionProviderInterface))

typedef struct _IdeCompletionProvider          IdeCompletionProvider;
typedef struct _IdeCompletionProviderInterface IdeCompletionProviderInterface;

struct _IdeCompletionProviderInterface
{
  GtkSourceCompletionProviderIface parent_interface;

  void (*load) (IdeCompletionProvider *self,
                IdeContext            *context);
};

GType     ide_completion_provider_get_type                     (void);
gboolean  ide_completion_provider_context_in_comment           (GtkSourceCompletionContext *context);
gboolean  ide_completion_provider_context_in_comment_or_string (GtkSourceCompletionContext *context);
gchar    *ide_completion_provider_context_current_word         (GtkSourceCompletionContext *context);
void      ide_completion_provider_load                         (IdeCompletionProvider      *self,
                                                                IdeContext                 *context);

G_END_DECLS

#endif /* IDE_COMPLETION_PROVIDER_H */
