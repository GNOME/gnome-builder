/* ide-application-tests.h
 *
 * Copyright 2016 Christian Hergert <christian@hergert.me>
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

#include "application/ide-application.h"

G_BEGIN_DECLS

typedef void     (*IdeApplicationTest)           (GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
typedef gboolean (*IdeApplicationTestCompletion) (GAsyncResult         *result,
                                                  GError              **error);

void ide_application_add_test (IdeApplication               *self,
                               const gchar                  *test_name,
                               IdeApplicationTest            test_func,
                               IdeApplicationTestCompletion  test_completion,
                               const gchar * const          *required_plugins);

G_END_DECLS
