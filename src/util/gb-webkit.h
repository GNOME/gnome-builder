/* gb-webkit.h
 *
 * Copyright (C) 2015 Michael Catanzaro <mcatanzaro@igalia.com>
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

#ifndef GB_WEBKIT_H
#define GB_WEBKIT_H

#include <webkit2/webkit2.h>

G_BEGIN_DECLS

void gb_webkit_web_view_apply_settings (WebKitWebView *view);

G_END_DECLS

#endif /* GB_WEBKIT_H */
