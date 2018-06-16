/* ctags-plugin.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include <libpeas/peas.h>
#include <gtksourceview/gtksource.h>

#include "ide-ctags-builder.h"
#include "ide-ctags-completion-item.h"
#include "ide-ctags-completion-provider.h"
#include "ide-ctags-highlighter.h"
#include "ide-ctags-index.h"
#include "ide-ctags-preferences-addin.h"
#include "ide-ctags-service.h"
#include "ide-ctags-symbol-resolver.h"

void _ide_ctags_index_register_type (GTypeModule *module);
void _ide_ctags_builder_register_type (GTypeModule *module);
void _ide_ctags_completion_provider_register_type (GTypeModule *module);
void _ide_ctags_highlighter_register_type (GTypeModule *module);
void _ide_ctags_service_register_type (GTypeModule *module);
void _ide_ctags_symbol_resolver_register_type (GTypeModule *module);

void
ide_ctags_register_types (PeasObjectModule *module)
{
  _ide_ctags_index_register_type (G_TYPE_MODULE (module));
  _ide_ctags_completion_provider_register_type (G_TYPE_MODULE (module));
  _ide_ctags_highlighter_register_type (G_TYPE_MODULE (module));
  _ide_ctags_service_register_type (G_TYPE_MODULE (module));
  _ide_ctags_symbol_resolver_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module, IDE_TYPE_COMPLETION_PROVIDER, IDE_TYPE_CTAGS_COMPLETION_PROVIDER);
  peas_object_module_register_extension_type (module, IDE_TYPE_HIGHLIGHTER, IDE_TYPE_CTAGS_HIGHLIGHTER);
  peas_object_module_register_extension_type (module, IDE_TYPE_SERVICE, IDE_TYPE_CTAGS_SERVICE);
  peas_object_module_register_extension_type (module, IDE_TYPE_PREFERENCES_ADDIN, IDE_TYPE_CTAGS_PREFERENCES_ADDIN);
  peas_object_module_register_extension_type (module, IDE_TYPE_SYMBOL_RESOLVER, IDE_TYPE_CTAGS_SYMBOL_RESOLVER);

  ide_vcs_register_ignored ("tags.??????");
}
