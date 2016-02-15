/* ide.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_H
#define IDE_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_INSIDE

#include "ide-types.h"

#include "ide-application.h"
#include "ide-application-addin.h"
#include "ide-application-tool.h"
#include "ide-back-forward-item.h"
#include "ide-back-forward-list.h"
#include "ide-build-result.h"
#include "ide-build-result-addin.h"
#include "ide-build-system.h"
#include "ide-builder.h"
#include "ide-buffer.h"
#include "ide-buffer-change-monitor.h"
#include "ide-buffer-manager.h"
#include "ide-completion-item.h"
#include "ide-completion-provider.h"
#include "ide-completion-results.h"
#include "ide-configuration.h"
#include "ide-configuration-manager.h"
#include "ide-context.h"
#include "ide-debug.h"
#include "ide-debugger.h"
#include "ide-deployer.h"
#include "ide-device.h"
#include "ide-device-provider.h"
#include "ide-device-manager.h"
#include "ide-diagnostic.h"
#include "ide-diagnostics.h"
#include "ide-diagnostician.h"
#include "ide-diagnostic-provider.h"
#include "ide-enums.h"
#include "ide-environment.h"
#include "ide-environment-editor.h"
#include "ide-environment-variable.h"
#include "ide-executable.h"
#include "ide-executer.h"
#include "ide-file.h"
#include "ide-file-settings.h"
#include "ide-global.h"
#include "ide-highlight-engine.h"
#include "ide-highlighter.h"
#include "ide-indenter.h"
#include "ide-layout-grid.h"
#include "ide-layout-pane.h"
#include "ide-layout-stack.h"
#include "ide-layout-view.h"
#include "ide-layout.h"
#include "ide-log.h"
#include "ide-macros.h"
#include "ide-object.h"
#include "ide-pattern-spec.h"
#include "ide-perspective.h"
#include "ide-preferences.h"
#include "ide-preferences-addin.h"
#include "ide-process.h"
#include "ide-progress.h"
#include "ide-project.h"
#include "ide-project-file.h"
#include "ide-project-files.h"
#include "ide-project-item.h"
#include "ide-recent-projects.h"
#include "ide-refactory.h"
#include "ide-runtime.h"
#include "ide-runtime-manager.h"
#include "ide-runtime-provider.h"
#include "ide-script.h"
#include "ide-script-manager.h"
#include "ide-search-context.h"
#include "ide-search-engine.h"
#include "ide-search-provider.h"
#include "ide-search-reducer.h"
#include "ide-search-result.h"
#include "ide-service.h"
#include "ide-source-location.h"
#include "ide-source-map.h"
#include "ide-source-range.h"
#include "ide-source-snippet-chunk.h"
#include "ide-source-snippet-context.h"
#include "ide-source-snippet.h"
#include "ide-source-snippets-manager.h"
#include "ide-source-snippets.h"
#include "ide-source-view.h"
#include "ide-subprocess-launcher.h"
#include "ide-symbol-resolver.h"
#include "ide-symbol.h"
#include "ide-target.h"
#include "ide-test-case.h"
#include "ide-test-suite.h"
#include "ide-thread-pool.h"
#include "ide-tree-types.h"
#include "ide-tree.h"
#include "ide-tree-builder.h"
#include "ide-tree-node.h"
#include "ide-types.h"
#include "ide-unsaved-file.h"
#include "ide-unsaved-files.h"
#include "ide-vcs.h"
#include "ide-vcs-uri.h"
#include "ide-workbench.h"
#include "ide-workbench-addin.h"
#include "ide-workbench-header-bar.h"

#include "editor/ide-editor-perspective.h"
#include "editor/ide-editor-view.h"
#include "editor/ide-editor-view-addin.h"

#include "genesis/ide-genesis-addin.h"

#include "doap/ide-doap-person.h"
#include "doap/ide-doap.h"
#include "local/ide-local-device.h"
#include "search/ide-omni-search-row.h"
#include "template/ide-project-template.h"
#include "template/ide-template-provider.h"
#include "util/ide-file-manager.h"
#include "util/ide-gdk.h"
#include "util/ide-gtk.h"
#include "util/ide-line-reader.h"
#include "util/ide-list-inline.h"

#undef IDE_INSIDE

G_END_DECLS

#endif /* IDE_H */
