/* gbp-snippet-application-addin.h
 *
 * Copyright 2022 Günther Wagner <info@gunibert.de>
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
 * SPDX-License-Gbpntifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define GBP_TYPE_SNIPPET_APPLICATION_ADDIN (gbp_snippet_application_addin_get_type())

G_DECLARE_FINAL_TYPE (GbpSnippetApplicationAddin, gbp_snippet_application_addin, GBP, SNIPPET_APPLICATION_ADDIN, GObject)

G_END_DECLS
