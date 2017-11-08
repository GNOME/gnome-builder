/* ide-completion-results.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#include <gtksourceview/gtksource.h>

#include "ide-version-macros.h"

#include "sourceview/ide-completion-item.h"

G_BEGIN_DECLS

#define IDE_TYPE_COMPLETION_RESULTS (ide_completion_results_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeCompletionResults, ide_completion_results, IDE, COMPLETION_RESULTS, GObject)

struct _IdeCompletionResultsClass
{
  GObjectClass parent_class;

  /**
   * IdeCompletionResults::compare:
   * @self: An #IdeCompletionResults
   * @left: An #IdeCompletionItem on the left hand side.
   * @right: An #IdeCompletionItem on the right hand side.
   *
   * Compares two completion items as they should be displayed.
   * See ide_completion_results_invalidate_sort() to invalide the
   * current sort settings.
   */
  gint (*compare) (IdeCompletionResults *self,
                   IdeCompletionItem    *left,
                   IdeCompletionItem    *right);
};

IDE_AVAILABLE_IN_ALL
IdeCompletionResults *ide_completion_results_new                    (const gchar                 *query);
IDE_AVAILABLE_IN_ALL
const gchar          *ide_completion_results_get_query              (IdeCompletionResults        *self);
IDE_AVAILABLE_IN_ALL
void                  ide_completion_results_invalidate_sort        (IdeCompletionResults        *self);
IDE_AVAILABLE_IN_ALL
void                  ide_completion_results_take_proposal          (IdeCompletionResults        *self,
                                                                     IdeCompletionItem           *proposal);
IDE_AVAILABLE_IN_ALL
void                  ide_completion_results_present                (IdeCompletionResults        *self,
                                                                     GtkSourceCompletionProvider *provider,
                                                                     GtkSourceCompletionContext  *context);
IDE_AVAILABLE_IN_ALL
gboolean              ide_completion_results_replay                 (IdeCompletionResults        *self,
                                                                     const gchar                 *query);
IDE_AVAILABLE_IN_ALL
guint                 ide_completion_results_get_size               (IdeCompletionResults        *self);
IDE_AVAILABLE_IN_ALL
gint                  ide_completion_results_get_insert_offset      (IdeCompletionResults        *self);
IDE_AVAILABLE_IN_ALL
gint                  ide_completion_results_get_end_offset         (IdeCompletionResults        *self);

G_END_DECLS
