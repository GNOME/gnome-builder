/* ide-deployer.c
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

#include "ide-deployer.h"

G_DEFINE_ABSTRACT_TYPE (IdeDeployer, ide_deployer, IDE_TYPE_OBJECT)

/*
 * TODO:
 *
 * This class is the base class for the code that will deploy a project to the
 * device where it can run. Locally, this might be a make install. On a remote
 * device it might be a combination of things.
 */

static void
ide_deployer_class_init (IdeDeployerClass *klass)
{
}

static void
ide_deployer_init (IdeDeployer *self)
{
}
