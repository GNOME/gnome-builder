/* registermmtype.h
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

#ifndef _IDE_REGISTERMMTYPE_H
#define _IDE_REGISTERMMTYPE_H

#include <glibmm/object.h>
#include <glibmm/private/object_p.h>
#include <glibmm/init.h>
#include <glibmm/property.h>

#include <initializer_list>

namespace Ide {

template<class DerivedCppType, class... Interfaces>
static GType register_mm_type(const gchar * type_name)
{
  Glib::init();

  struct GlibCppType
  {
    typename DerivedCppType::BaseObjectType parent;
    DerivedCppType *self;

    static void init(GlibCppType *instance, gpointer /* g_class */)
    {
      instance->self = new DerivedCppType(&instance->parent);
    }

    static void finalize(GObject *object)
    {
      (G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(object))))->finalize(object);
    }
  };

  struct GlibCppTypeClass
  {
    static void init (GlibCppTypeClass * klass, gpointer data)
    {
      DerivedCppType::CppClassType::class_init_function(klass, data);
	
      GObjectClass *gobject_class = reinterpret_cast<GObjectClass*>(klass);
      gobject_class->get_property = &Glib::custom_get_property_callback;
      gobject_class->set_property = &Glib::custom_set_property_callback;
      gobject_class->finalize =  &GlibCppType::finalize;
    }
  };

  GType parent_type = DerivedCppType::get_base_type();
  static volatile gsize gonce_data = 0;

  if (g_once_init_enter (&gonce_data)) {
    GTypeInfo info;

    info.class_size = sizeof(typename DerivedCppType::BaseClassType);
    info.base_init = nullptr;
    info.base_finalize = nullptr;
    info.class_init = (GClassInitFunc) &GlibCppTypeClass::init;
    info.class_finalize = nullptr;
    info.class_data = nullptr;
    info.instance_size = sizeof(GlibCppType);
    info.n_preallocs = 0;
    info.instance_init = (GInstanceInitFunc) &GlibCppType::init;
    info.value_table = nullptr;

    GType _type = g_type_register_static(parent_type, type_name, &info, (GTypeFlags)0);
    g_once_init_leave(&gonce_data, (gsize) _type);

    std::initializer_list<int> { (Interfaces::add_interface(_type), 0)... }; 
  }
    
  return (GType) gonce_data;
}

}
 
#endif
