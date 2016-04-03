/* generate_defs_libide.cc
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

#include <glibmm_generate_extra_defs/generate_extra_defs.h>

#include <ide.h>

bool libide_type_is_a_pointer(GType gtype)
{
  return (gtype_is_a_pointer(gtype));
}

int main (int argc, char *argv[])
{

  std::cout << get_defs(IDE_TYPE_APPLICATION, libide_type_is_a_pointer);
  
  return 0;
}
