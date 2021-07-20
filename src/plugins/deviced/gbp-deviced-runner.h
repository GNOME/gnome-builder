/* gbp-deviced-runner.h
 *
 * Copyright 2021 James Westman <james@jwestman.net>
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

#include <libide-foundry.h>
#include "gbp-deviced-device.h"

G_BEGIN_DECLS

#define GBP_TYPE_DEVICED_RUNNER (gbp_deviced_runner_get_type())

G_DECLARE_FINAL_TYPE (GbpDevicedRunner, gbp_deviced_runner, GBP, DEVICED_RUNNER, IdeRunner)

GbpDevicedRunner *gbp_deviced_runner_new (GbpDevicedDevice *device);

GbpDevicedDevice *gbp_deviced_runner_get_device (GbpDevicedRunner *self);
void              gbp_deviced_runner_set_device (GbpDevicedRunner *self,
                                                 GbpDevicedDevice *device);

G_END_DECLS
