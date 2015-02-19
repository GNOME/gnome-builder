/* ide-clang-symbol-resolver.c
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

#include "ide-clang-symbol-resolver.h"

struct _IdeClangSymbolResolver
{
  IdeSymbolResolver parent_instance;
};

G_DEFINE_TYPE (IdeClangSymbolResolver, ide_clang_symbol_resolver, IDE_TYPE_SYMBOL_RESOLVER)

static void
ide_clang_symbol_resolver_class_init (IdeClangSymbolResolverClass *klass)
{
}

static void
ide_clang_symbol_resolver_init (IdeClangSymbolResolver *self)
{
}
