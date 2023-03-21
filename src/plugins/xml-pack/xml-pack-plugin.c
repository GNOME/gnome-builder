/* xml-pack-plugin.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <libpeas.h>

#include <libide-code.h>
#include <libide-sourceview.h>

#include "ide-xml-completion-provider.h"
#include "ide-xml-diagnostic-provider.h"
#include "ide-xml-formatter.h"
#include "ide-xml-highlighter.h"
#include "ide-xml-indenter.h"
#include "ide-xml-symbol-resolver.h"

_IDE_EXTERN void
_ide_xml_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                              IDE_TYPE_XML_COMPLETION_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                              IDE_TYPE_XML_DIAGNOSTIC_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_FORMATTER,
                                              IDE_TYPE_XML_FORMATTER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_HIGHLIGHTER,
                                              IDE_TYPE_XML_HIGHLIGHTER);
  peas_object_module_register_extension_type (module,
                                              GTK_SOURCE_TYPE_INDENTER,
                                              IDE_TYPE_XML_INDENTER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SYMBOL_RESOLVER,
                                              IDE_TYPE_XML_SYMBOL_RESOLVER);
}
