/* ide-service.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include "ide-version-macros.h"

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_SERVICE (ide_service_get_type())

G_DECLARE_INTERFACE (IdeService, ide_service, IDE, SERVICE, IdeObject)

/**
 * IdeServiceInterface:
 * @context_loaded: Implement this virtual function to be notified when the
 *   #IdeContext has completed loading.
 * @get_name: Implement this virtual function to provide a useful name of
 *   the service. By default, the type name is used.
 * @start: Implement this virtual function be notified when the service
 *   should start processing. This is usually before @context_loaded has
 *   been called and services must deal with that.
 * @stop: Implement this virtual function to be notified when the service
 *   should shut itself down by cleaning up any resources.
 *
 * Since: 3.16
 */

struct _IdeServiceInterface
{
  GTypeInterface parent_interface;

  void         (*context_loaded) (IdeService *self);
  const gchar *(*get_name)       (IdeService *self);
  void         (*start)          (IdeService *self);
  void         (*stop)           (IdeService *self);
};

IDE_AVAILABLE_IN_ALL
const gchar *ide_service_get_name             (IdeService *self);
IDE_AVAILABLE_IN_ALL
void         ide_service_start                (IdeService *self);
IDE_AVAILABLE_IN_ALL
void         ide_service_stop                 (IdeService *self);
void         _ide_service_emit_context_loaded (IdeService *self) G_GNUC_INTERNAL;

G_END_DECLS
