/* ide-langserv-types.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if !defined (IDE_LSP_INSIDE) && !defined (IDE_LSP_COMPILATION)
# error "Only <libide-lsp.h> can be included directly."
#endif

G_BEGIN_DECLS

typedef enum
{
	IDE_LSP_COMPLETION_TEXT           = 1,
	IDE_LSP_COMPLETION_METHOD         = 2,
	IDE_LSP_COMPLETION_FUNCTION       = 3,
	IDE_LSP_COMPLETION_CONSTRUCTOR    = 4,
	IDE_LSP_COMPLETION_FIELD          = 5,
	IDE_LSP_COMPLETION_VARIABLE       = 6,
	IDE_LSP_COMPLETION_CLASS          = 7,
	IDE_LSP_COMPLETION_INTERFACE      = 8,
	IDE_LSP_COMPLETION_MODULE         = 9,
	IDE_LSP_COMPLETION_PROPERTY       = 10,
	IDE_LSP_COMPLETION_UNIT           = 11,
	IDE_LSP_COMPLETION_VALUE          = 12,
	IDE_LSP_COMPLETION_ENUM           = 13,
	IDE_LSP_COMPLETION_KEYWORD        = 14,
	IDE_LSP_COMPLETION_SNIPPET        = 15,
	IDE_LSP_COMPLETION_COLOR          = 16,
	IDE_LSP_COMPLETION_FILE           = 17,
	IDE_LSP_COMPLETION_REFERENCE      = 18,
	IDE_LSP_COMPLETION_FOLDER         = 19,
	IDE_LSP_COMPLETION_ENUM_MEMBER    = 20,
	IDE_LSP_COMPLETION_CONSTANT       = 21,
	IDE_LSP_COMPLETION_STRUCT         = 22,
	IDE_LSP_COMPLETION_EVENT          = 23,
	IDE_LSP_COMPLETION_OPERATOR       = 24,
	IDE_LSP_COMPLETION_TYPE_PARAMETER = 25,
} IdeLspCompletionKind;

G_END_DECLS
