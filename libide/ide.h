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

#include "application/ide-application-addin.h"
#include "application/ide-application-tool.h"
#include "application/ide-application.h"
#include "buffers/ide-buffer-change-monitor.h"
#include "buffers/ide-buffer-manager.h"
#include "buffers/ide-buffer.h"
#include "buffers/ide-unsaved-file.h"
#include "buffers/ide-unsaved-files.h"
#include "buildsystem/ide-build-manager.h"
#include "buildsystem/ide-build-result-addin.h"
#include "buildsystem/ide-build-result.h"
#include "buildsystem/ide-build-system.h"
#include "buildsystem/ide-build-target.h"
#include "buildsystem/ide-builder.h"
#include "buildsystem/ide-configuration-manager.h"
#include "buildsystem/ide-configuration.h"
#include "buildsystem/ide-environment-variable.h"
#include "buildsystem/ide-environment.h"
#include "devices/ide-device-manager.h"
#include "devices/ide-device-provider.h"
#include "devices/ide-device.h"
#include "diagnostics/ide-diagnostic-provider.h"
#include "diagnostics/ide-diagnostic.h"
#include "diagnostics/ide-diagnostician.h"
#include "diagnostics/ide-diagnostics.h"
#include "diagnostics/ide-source-location.h"
#include "diagnostics/ide-source-range.h"
#include "doap/ide-doap-person.h"
#include "doap/ide-doap.h"
#include "editor/ide-editor-perspective.h"
#include "editor/ide-editor-view-addin.h"
#include "editor/ide-editor-view.h"
#include "files/ide-file-settings.h"
#include "files/ide-file.h"
#include "genesis/ide-genesis-addin.h"
#include "highlighting/ide-highlight-engine.h"
#include "highlighting/ide-highlight-index.h"
#include "highlighting/ide-highlighter.h"
#include "history/ide-back-forward-item.h"
#include "history/ide-back-forward-list.h"
#include "ide-context.h"
#include "ide-debug.h"
#include "ide-enums.h"
#include "ide-global.h"
#include "ide-macros.h"
#include "ide-object.h"
#include "ide-service.h"
#include "ide-types.h"
#include "local/ide-local-device.h"
#include "logging/ide-log.h"
#include "preferences/ide-preferences-addin.h"
#include "preferences/ide-preferences.h"
#include "projects/ide-project-file.h"
#include "projects/ide-project-files.h"
#include "projects/ide-project-item.h"
#include "projects/ide-project-miner.h"
#include "projects/ide-project.h"
#include "projects/ide-recent-projects.h"
#include "runner/ide-run-manager.h"
#include "runner/ide-runner.h"
#include "runner/ide-runner-addin.h"
#include "runtimes/ide-runtime-manager.h"
#include "runtimes/ide-runtime-provider.h"
#include "runtimes/ide-runtime.h"
#include "scripting/ide-script-manager.h"
#include "scripting/ide-script.h"
#include "search/ide-omni-search-row.h"
#include "search/ide-pattern-spec.h"
#include "search/ide-search-context.h"
#include "search/ide-search-engine.h"
#include "search/ide-search-provider.h"
#include "search/ide-search-reducer.h"
#include "search/ide-search-result.h"
#include "snippets/ide-source-snippet-chunk.h"
#include "snippets/ide-source-snippet-context.h"
#include "snippets/ide-source-snippet.h"
#include "snippets/ide-source-snippets-manager.h"
#include "snippets/ide-source-snippets.h"
#include "sourceview/ide-completion-item.h"
#include "sourceview/ide-completion-provider.h"
#include "sourceview/ide-completion-results.h"
#include "sourceview/ide-indenter.h"
#include "sourceview/ide-language.h"
#include "sourceview/ide-source-map.h"
#include "sourceview/ide-source-style-scheme.h"
#include "sourceview/ide-source-view.h"
#include "symbols/ide-symbol-resolver.h"
#include "symbols/ide-symbol.h"
#include "symbols/ide-tags-builder.h"
#include "template/ide-project-template.h"
#include "template/ide-template-base.h"
#include "template/ide-template-provider.h"
#include "threading/ide-thread-pool.h"
#include "tree/ide-tree-builder.h"
#include "tree/ide-tree-node.h"
#include "tree/ide-tree-types.h"
#include "tree/ide-tree.h"
#include "util/ide-file-manager.h"
#include "util/ide-gtk.h"
#include "util/ide-line-reader.h"
#include "util/ide-list-inline.h"
#include "util/ide-posix.h"
#include "util/ide-progress.h"
#include "util/ide-ref-ptr.h"
#include "util/ide-uri.h"
#include "vcs/ide-vcs-config.h"
#include "vcs/ide-vcs-initializer.h"
#include "vcs/ide-vcs-uri.h"
#include "vcs/ide-vcs.h"
#include "workbench/ide-layout-grid.h"
#include "workbench/ide-layout-pane.h"
#include "workbench/ide-layout-stack.h"
#include "workbench/ide-layout-view.h"
#include "workbench/ide-layout.h"
#include "workbench/ide-perspective.h"
#include "workbench/ide-workbench-addin.h"
#include "workbench/ide-workbench-header-bar.h"
#include "workbench/ide-workbench.h"
#include "workers/ide-subprocess-launcher.h"

#undef IDE_INSIDE

G_END_DECLS

#endif /* IDE_H */
