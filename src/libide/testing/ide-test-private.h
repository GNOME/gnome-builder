/* ide-test-private.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <gtk/gtk.h>

#include "ide-test.h"
#include "ide-test-manager.h"
#include "ide-test-provider.h"

G_BEGIN_DECLS

typedef enum
{
  IDE_TEST_COLUMN_GROUP,
  IDE_TEST_COLUMN_TEST,
} IdeTestColumn;

G_GNUC_INTERNAL GtkTreeModel    *_ide_test_manager_get_model (IdeTestManager  *self);
G_GNUC_INTERNAL void             _ide_test_set_provider      (IdeTest         *self,
                                                              IdeTestProvider *provider);
G_GNUC_INTERNAL IdeTestProvider *_ide_test_get_provider      (IdeTest         *self);


G_END_DECLS
