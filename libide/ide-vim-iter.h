/* ide-vim-iter.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_VIM_ITER_H
#define IDE_VIM_ITER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

gboolean _ide_vim_iter_backward_paragraph_start (GtkTextIter *iter);
gboolean _ide_vim_iter_forward_paragraph_end    (GtkTextIter *iter);
gboolean _ide_vim_iter_backward_sentence_start  (GtkTextIter *iter);
gboolean _ide_vim_iter_forward_sentence_end     (GtkTextIter *iter);

G_END_DECLS

#endif /* IDE_VIM_ITER_H */
