/* ide-cross-compilation-device-provider.h
 *
 * Copyright (C) 2018 Corentin Noël <corentin.noel@collabora.co.uk>
 * Copyright (C) 2018 Collabora Ltd.
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

#ifndef IDE_CROSS_COMPILATION_DEVICE_PROVIDER_H
#define IDE_CROSS_COMPILATION_DEVICE_PROVIDER_H

#include <ide.h>

G_BEGIN_DECLS

#define IDE_TYPE_CROSS_COMPILATION_DEVICE_PROVIDER (ide_cross_compilation_device_provider_get_type())

G_DECLARE_FINAL_TYPE (IdeCrossCompilationDeviceProvider, ide_cross_compilation_device_provider, IDE, CROSS_COMPILATION_DEVICE_PROVIDER, IdeObject)

G_END_DECLS

#endif /* IDE_CROSS_COMPILATION_DEVICE_PROVIDER_H */
