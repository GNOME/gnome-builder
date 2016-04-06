/* hellocppplugin.cc
 *
 * Copyright (C) 2016 Marcin Kolny <marcin.kolny@gmail.com>
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

#include <idemm/registermmtype.h>

#include "hellocppapplicationaddin.h"

#include <libpeas/peas.h>

G_BEGIN_DECLS

void
peas_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              Ide::ApplicationAddin::get_base_type(),
                                              Ide::register_mm_type<Ide::Plugin::HelloCppApplicationAddin, Ide::ApplicationAddin>("HelloCppApplicationAddin"));
}

G_END_DECLS
