/* gb-webkit.c
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

#include "gb-webkit.h"

void
gb_webkit_web_view_apply_settings (WebKitWebView *view)
{
  g_return_if_fail (WEBKIT_IS_WEB_VIEW (view));

  /*
   * TODO: Consider whether HTML5 local storage should be enabled. It could only
   * possibly be useful for complex web sites, but if you're building a web site
   * with Builder then it might be useful to have. But leave disabled until
   * https://bugs.webkit.org/show_bug.cgi?id=143245 has been fixed.
   */
  g_object_set (webkit_web_view_get_settings (view),
                "enable-html5-local-storage", FALSE,
                NULL);
}
