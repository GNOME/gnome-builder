/* ide-back-forward-list.h
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

#ifndef IDE_BACK_FORWARD_LIST_H
#define IDE_BACK_FORWARD_LIST_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_BACK_FORWARD_LIST (ide_back_forward_list_get_type())

G_DECLARE_FINAL_TYPE (IdeBackForwardList, ide_back_forward_list, IDE, BACK_FORWARD_LIST, IdeObject)

struct _IdeBackForwardList
{
  IdeObject parent_instance;
};

void                ide_back_forward_list_go_backward         (IdeBackForwardList *self);
void                ide_back_forward_list_go_forward          (IdeBackForwardList *self);
gboolean            ide_back_forward_list_get_can_go_backward (IdeBackForwardList *self);
gboolean            ide_back_forward_list_get_can_go_forward  (IdeBackForwardList *self);
void                ide_back_forward_list_push                (IdeBackForwardList *self,
                                                               IdeBackForwardItem *item);
IdeBackForwardList *ide_back_forward_list_branch              (IdeBackForwardList *self);
void                ide_back_forward_list_merge               (IdeBackForwardList *self,
                                                               IdeBackForwardList *branch);

G_END_DECLS

#endif /* IDE_BACK_FORWARD_LIST_H */
