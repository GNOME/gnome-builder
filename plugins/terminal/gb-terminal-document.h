/* gb-terminal-document.h
 *
 * Copyright (C) 2015 Sebastien Lafargue <slafargue@gnome.org>
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

#ifndef GB_TERMINAL_DOCUMENT_H
#define GB_TERMINAL_DOCUMENT_H

#include "gb-document.h"

G_BEGIN_DECLS

#define GB_TYPE_TERMINAL_DOCUMENT (gb_terminal_document_get_type())

G_DECLARE_FINAL_TYPE (GbTerminalDocument, gb_terminal_document, GB, TERMINAL_DOCUMENT, GObject)

GbTerminalDocument  *gb_terminal_document_new          (void);
void                 gb_terminal_document_set_title    (GbTerminalDocument *document,
                                                        const gchar        *title);

G_END_DECLS

#endif /* GB_TERMINAL_DOCUMENT_H */
