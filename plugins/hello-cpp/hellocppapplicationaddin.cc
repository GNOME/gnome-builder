/* hellocppapplicationaddin.cc
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

#include "hellocppapplicationaddin.h"

#include <gtkmm/application.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/main.h>

#include <glib/gi18n.h>

#include <iostream>
#include <cstdio>
namespace Ide {
namespace Plugin {

HelloCppApplicationAddin::HelloCppApplicationAddin(GObject *gobj)
  : Glib::ObjectBase(typeid(HelloCppApplicationAddin)),
    Glib::Object(gobj),
    Ide::ApplicationAddin()
{
  Gtk::Main::init_gtkmm_internals();
  Ide::wrap_init();
}

  void HelloCppApplicationAddin::load_vfunc(const Glib::RefPtr<Ide::Application>& application)
{
  using namespace std::chrono;

  application->signal_shutdown().connect([this] {
      auto stop = steady_clock::now();
      auto elapsed_seconds = duration_cast<seconds>(stop - start).count();
      char message [100];
      snprintf(message, 100, N_("Wow! You've spent with Builder %d seconds!"), elapsed_seconds);
      Gtk::MessageDialog(gettext(message)).run();
    });

  start = std::chrono::steady_clock::now();
}

  void HelloCppApplicationAddin::unload_vfunc(const Glib::RefPtr<Ide::Application>& application)
{
  std::cout << "Unloading application" << std::endl;
}

}
}
