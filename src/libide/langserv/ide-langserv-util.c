/* ide-langserv-util.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#include "langserv/ide-langserv-util.h"

IdeSymbolKind
ide_langserv_decode_symbol_kind (guint kind)
{
  switch (kind)
    {
    case 1:   kind = IDE_SYMBOL_FILE;         break;
    case 2:   kind = IDE_SYMBOL_MODULE;       break;
    case 3:   kind = IDE_SYMBOL_NAMESPACE;    break;
    case 4:   kind = IDE_SYMBOL_PACKAGE;      break;
    case 5:   kind = IDE_SYMBOL_CLASS;        break;
    case 6:   kind = IDE_SYMBOL_METHOD;       break;
    case 7:   kind = IDE_SYMBOL_PROPERTY;     break;
    case 8:   kind = IDE_SYMBOL_FIELD;        break;
    case 9:   kind = IDE_SYMBOL_CONSTRUCTOR;  break;
    case 10:  kind = IDE_SYMBOL_ENUM;         break;
    case 11:  kind = IDE_SYMBOL_INTERFACE;    break;
    case 12:  kind = IDE_SYMBOL_FUNCTION;     break;
    case 13:  kind = IDE_SYMBOL_VARIABLE;     break;
    case 14:  kind = IDE_SYMBOL_CONSTANT;     break;
    case 15:  kind = IDE_SYMBOL_STRING;       break;
    case 16:  kind = IDE_SYMBOL_NUMBER;       break;
    case 17:  kind = IDE_SYMBOL_BOOLEAN;      break;
    case 18:  kind = IDE_SYMBOL_ARRAY;        break;
    default:  kind = IDE_SYMBOL_NONE;         break;
    }

  return kind;
}

IdeSymbolKind
ide_langserv_decode_completion_kind (guint kind)
{
  switch (kind)
    {
    case 1:   kind = IDE_SYMBOL_STRING;       break;
    case 2:   kind = IDE_SYMBOL_METHOD;       break;
    case 3:   kind = IDE_SYMBOL_FUNCTION;     break;
    case 4:   kind = IDE_SYMBOL_CONSTRUCTOR;  break;
    case 5:   kind = IDE_SYMBOL_FIELD;        break;
    case 6:   kind = IDE_SYMBOL_VARIABLE;     break;
    case 7:   kind = IDE_SYMBOL_CLASS;        break;
    case 8:   kind = IDE_SYMBOL_INTERFACE;    break;
    case 9:   kind = IDE_SYMBOL_MODULE;       break;
    case 10:  kind = IDE_SYMBOL_PROPERTY;     break;
    case 11:  kind = IDE_SYMBOL_NUMBER;       break;
    case 12:  kind = IDE_SYMBOL_SCALAR;       break;
    case 13:  kind = IDE_SYMBOL_ENUM_VALUE;   break;
    case 14:  kind = IDE_SYMBOL_KEYWORD;      break;
    case 17:  kind = IDE_SYMBOL_FILE;         break;

    case 15: /* Snippet */
    case 16: /* Color */
    case 18: /* Reference */
    default:  kind = IDE_SYMBOL_NONE;         break;
    }

  return kind;
}
