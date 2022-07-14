/* ide-test.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <vte/vte.h>

#include <libide-core.h>

#include "ide-foundry-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_TEST (ide_test_get_type())

typedef enum
{
  IDE_TEST_STATUS_NONE,
  IDE_TEST_STATUS_RUNNING,
  IDE_TEST_STATUS_SUCCESS,
  IDE_TEST_STATUS_FAILED,
} IdeTestStatus;

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeTest, ide_test, IDE, TEST, GObject)

IDE_AVAILABLE_IN_ALL
IdeTest       *ide_test_new             (IdeRunCommand        *run_command);
IDE_AVAILABLE_IN_ALL
const char    *ide_test_get_id          (IdeTest              *self);
IDE_AVAILABLE_IN_ALL
IdeTestStatus  ide_test_get_status      (IdeTest              *self);
IDE_AVAILABLE_IN_ALL
const char    *ide_test_get_title       (IdeTest              *self);
IDE_AVAILABLE_IN_ALL
const char    *ide_test_get_icon_name   (IdeTest              *self);
IDE_AVAILABLE_IN_ALL
IdeRunCommand *ide_test_get_run_command (IdeTest              *self);

G_END_DECLS
