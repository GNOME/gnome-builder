/* ide.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_H
#define IDE_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_INSIDE

#include "ide-back-forward-item.h"
#include "ide-back-forward-list.h"
#include "ide-buffer.h"
#include "ide-buffer-iter.h"
#include "ide-build-result.h"
#include "ide-build-system.h"
#include "ide-builder.h"
#include "ide-context.h"
#include "ide-debugger.h"
#include "ide-deployer.h"
#include "ide-device.h"
#include "ide-device-provider.h"
#include "ide-device-manager.h"
#include "ide-diagnostic.h"
#include "ide-diagnostician.h"
#include "ide-diagnostic-provider.h"
#include "ide-executable.h"
#include "ide-executer.h"
#include "ide-file.h"
#include "ide-global.h"
#include "ide-highlighter.h"
#include "ide-indenter.h"
#include "ide-language.h"
#include "ide-object.h"
#include "ide-process.h"
#include "ide-project.h"
#include "ide-project-file.h"
#include "ide-project-files.h"
#include "ide-project-item.h"
#include "ide-refactory.h"
#include "ide-script.h"
#include "ide-search-engine.h"
#include "ide-search-provider.h"
#include "ide-search-result.h"
#include "ide-service.h"
#include "ide-source-location.h"
#include "ide-symbol-resolver.h"
#include "ide-symbol.h"
#include "ide-target.h"
#include "ide-test-case.h"
#include "ide-test-suite.h"
#include "ide-types.h"
#include "ide-unsaved-files.h"
#include "ide-vcs.h"

#include "autotools/ide-autotools-build-system.h"
#include "directory/ide-directory-build-system.h"
#include "directory/ide-directory-vcs.h"
#include "git/ide-git-vcs.h"
#include "local/ide-local-device.h"

#undef IDE_INSIDE

G_END_DECLS

#endif /* IDE_H */
