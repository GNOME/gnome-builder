/* ide-view.h
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

#ifndef IDE_VIEW_H
#define IDE_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define IDE_TYPE_VIEW (ide_view_get_type())

G_DECLARE_INTERFACE (IdeView, ide_view, IDE, VIEW, GtkWidget)

struct _IdeViewInterface
{
  GTypeInterface parent;

  const gchar *(*get_title)           (IdeView              *self);
  const gchar *(*get_icon_name)       (IdeView              *self);
  gboolean     (*get_can_save)        (IdeView              *self);
  gboolean     (*get_needs_attention) (IdeView              *self);
  void         (*save_async)          (IdeView              *self,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);
  gboolean     (*save_finish)         (IdeView              *self,
                                       GAsyncResult         *result,
                                       GError              **error);
};

const gchar *ide_view_get_title           (IdeView              *self);
const gchar *ide_view_get_icon_name       (IdeView              *self);
gboolean     ide_view_get_can_save        (IdeView              *self);
gboolean     ide_view_get_needs_attention (IdeView              *self);
void         ide_view_save_async          (IdeView              *self,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data);
gboolean     ide_view_save_finish         (IdeView              *self,
                                           GAsyncResult         *result,
                                           GError              **error);

G_END_DECLS

#endif /* IDE_VIEW_H */
