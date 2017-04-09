/* mi2-glib.h
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

#ifndef MI2_GLIB_H
#define MI2_GLIB_H

#include <gio/gio.h>

G_BEGIN_DECLS

#include "mi2-client.h"
#include "mi2-command-message.h"
#include "mi2-console-message.h"
#include "mi2-event-message.h"
#include "mi2-info-message.h"
#include "mi2-input-stream.h"
#include "mi2-message.h"

G_END_DECLS

#endif /* MI2_GLIB_H */
