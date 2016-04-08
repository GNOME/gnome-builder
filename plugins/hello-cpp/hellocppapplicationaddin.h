/* hellocppapplicationaddin.h
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

#ifndef _IDE_PLUGIN_HELLOCPPAPPLICATIONADDIN_H
#define _IDE_PLUGIN_HELLOCPPAPPLICATIONADDIN_H

#include <idemm.h>

#include <chrono>

namespace Ide {
namespace Plugin {

class HelloCppApplicationAddin : public Glib::Object, public Ide::ApplicationAddin
{
private:
  std::chrono::time_point<std::chrono::steady_clock> start;
  sigc::connection con;
  
public:
  typedef GObject BaseObjectType;
  typedef Glib::Object_Class CppClassType;
  typedef Glib::Object::BaseClassType BaseClassType;

  static GType get_base_type() { return Glib::Object::get_base_type(); }
  
  explicit HelloCppApplicationAddin(GObject *gobj);

  void load_vfunc(const Glib::RefPtr<Ide::Application>& application) override;
  void unload_vfunc(const Glib::RefPtr<Ide::Application>& application) override;
};

}
}

#endif
