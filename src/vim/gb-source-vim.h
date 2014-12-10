/* gb-source-vim.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#ifndef GB_SOURCE_VIM_H
#define GB_SOURCE_VIM_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_VIM            (gb_source_vim_get_type())
#define GB_TYPE_SOURCE_VIM_MODE       (gb_source_vim_mode_get_type())
#define GB_SOURCE_VIM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_VIM, GbSourceVim))
#define GB_SOURCE_VIM_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_VIM, GbSourceVim const))
#define GB_SOURCE_VIM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_VIM, GbSourceVimClass))
#define GB_IS_SOURCE_VIM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_VIM))
#define GB_IS_SOURCE_VIM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_VIM))
#define GB_SOURCE_VIM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_VIM, GbSourceVimClass))

typedef struct _GbSourceVim        GbSourceVim;
typedef struct _GbSourceVimClass   GbSourceVimClass;
typedef struct _GbSourceVimPrivate GbSourceVimPrivate;

typedef enum
{
  GB_SOURCE_VIM_NORMAL = 1,
  GB_SOURCE_VIM_INSERT,
  GB_SOURCE_VIM_COMMAND,
} GbSourceVimMode;

struct _GbSourceVim
{
  GObject parent;

  /*< private >*/
  GbSourceVimPrivate *priv;
};

struct _GbSourceVimClass
{
  GObjectClass parent_class;

  void (*begin_search)               (GbSourceVim *vim,
                                      const gchar *search_text);
  void (*command_visibility_toggled) (GbSourceVim *vim,
                                      gboolean     visibility);
  void (*jump_to_doc)                (GbSourceVim *vim,
                                      const gchar *search_text);

  gpointer _padding1;
  gpointer _padding2;
  gpointer _padding3;
  gpointer _padding4;
  gpointer _padding5;
};

GType            gb_source_vim_get_type        (void) G_GNUC_CONST;
GType            gb_source_vim_mode_get_type   (void) G_GNUC_CONST;
GbSourceVim     *gb_source_vim_new             (GtkTextView     *text_view);
GbSourceVimMode  gb_source_vim_get_mode        (GbSourceVim     *vim);
void             gb_source_vim_set_mode        (GbSourceVim     *vim,
                                                GbSourceVimMode  mode);
const gchar     *gb_source_vim_get_phrase      (GbSourceVim     *vim);
gboolean         gb_source_vim_get_enabled     (GbSourceVim     *vim);
void             gb_source_vim_set_enabled     (GbSourceVim     *vim,
                                                gboolean         enabled);
GtkWidget       *gb_source_vim_get_text_view   (GbSourceVim     *vim);
gboolean         gb_source_vim_execute_command (GbSourceVim     *vim,
                                                const gchar     *command);
gboolean         gb_source_vim_is_command      (const gchar     *command_text);
gchar           *gb_source_vim_get_current_word (GbSourceVim     *vim,
                                                 GtkTextIter     *begin,
                                                 GtkTextIter     *end);

G_END_DECLS

#endif /* GB_SOURCE_VIM_H */
