/* ide-back-forward-list-private.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_BACK_FORWARD_LIST_PRIVATE_H
#define IDE_BACK_FORWARD_LIST_PRIVATE_H

#include <gio/gio.h>

#include "ide-back-forward-list.h"

G_BEGIN_DECLS

void                _ide_back_forward_list_foreach     (IdeBackForwardList    *self,
                                                        GFunc                  callback,
                                                        gpointer               user_data);
void                _ide_back_forward_list_load_async  (IdeBackForwardList    *self,
                                                        GFile                 *file,
                                                        GCancellable          *cancellable,
                                                        GAsyncReadyCallback    callback,
                                                        gpointer               user_data);
gboolean            _ide_back_forward_list_load_finish (IdeBackForwardList    *self,
                                                        GAsyncResult          *result,
                                                        GError               **error);
void                _ide_back_forward_list_save_async  (IdeBackForwardList    *self,
                                                        GFile                 *file,
                                                        GCancellable          *cancellable,
                                                        GAsyncReadyCallback    callback,
                                                        gpointer               user_data);
gboolean            _ide_back_forward_list_save_finish (IdeBackForwardList    *self,
                                                        GAsyncResult          *result,
                                                        GError               **error);
IdeBackForwardItem *_ide_back_forward_list_find        (IdeBackForwardList    *self,
                                                        IdeFile               *file);

G_END_DECLS

#endif /* IDE_BACK_FORWARD_LIST_PRIVATE_H */
