/*
 * modelie-parser.h
 * Emacs, Kate and Vim-style modelines support for gedit.
 *
 * Copyright 2005-2007 - Steve Frécinaux <code@istique.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MODELINE_PARSER_H__
#define __MODELINE_PARSER_H__

#include <glib.h>
#include <gtksourceview/gtksource.h>
#include <libide-code.h>

G_BEGIN_DECLS

void modeline_parser_init           (void);
void modeline_parser_shutdown       (void);
void modeline_parser_apply_modeline (GtkTextBuffer   *buffer,
                                     IdeFileSettings *file_settings);

G_END_DECLS

#endif /* __MODELINE_PARSER_H__ */
/* ex:set ts=8 noet: */
