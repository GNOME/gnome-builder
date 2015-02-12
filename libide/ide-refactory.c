/* ide-refactory.c
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

#include "ide-refactory.h"

G_DEFINE_ABSTRACT_TYPE (IdeRefactory, ide_refactory, G_TYPE_OBJECT)

/*
 * TODO:
 *
 * If you'd like to work on the refactory engine, ping me.
 *
 * Examples would include:
 *
 *   - Rename method, local, etc
 *   - Extract into method
 *   - Rename GObject
 *   - Whatever else you like.
 */

static void
ide_refactory_class_init (IdeRefactoryClass *klass)
{
}

static void
ide_refactory_init (IdeRefactory *self)
{
}
