/* ide.h
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

#pragma once

#include <dazzle.h>

G_BEGIN_DECLS

#define IDE_INSIDE

#include "ide-types.h"

#include "ide-context.h"
#include "ide-debug.h"
#include "ide-enums.h"
#include "ide-global.h"
#include "ide-object.h"
#include "ide-pausable.h"
#include "ide-service.h"
#include "ide-version.h"
#include "ide-version-macros.h"

#include "application/ide-application-addin.h"
#include "application/ide-application-tool.h"
#include "application/ide-application.h"
#include "buffers/ide-buffer-addin.h"
#include "buffers/ide-buffer-change-monitor.h"
#include "buffers/ide-buffer-manager.h"
#include "buffers/ide-buffer.h"
#include "buffers/ide-unsaved-file.h"
#include "buffers/ide-unsaved-files.h"
#include "buildconfig/ide-buildconfig-configuration.h"
#include "buildsystem/ide-build-manager.h"
#include "buildsystem/ide-build-pipeline.h"
#include "buildsystem/ide-build-pipeline-addin.h"
#include "buildsystem/ide-build-stage.h"
#include "buildsystem/ide-build-stage-launcher.h"
#include "buildsystem/ide-build-stage-mkdirs.h"
#include "buildsystem/ide-build-stage-transfer.h"
#include "buildsystem/ide-build-system.h"
#include "buildsystem/ide-build-system-discovery.h"
#include "buildsystem/ide-build-target.h"
#include "buildsystem/ide-build-target-provider.h"
#include "buildsystem/ide-compile-commands.h"
#include "buildsystem/ide-dependency-updater.h"
#include "buildsystem/ide-environment-variable.h"
#include "buildsystem/ide-environment.h"
#include "buildsystem/ide-simple-build-target.h"
#include "completion/ide-completion-compat.h"
#include "completion/ide-completion-context.h"
#include "completion/ide-completion-item.h"
#include "completion/ide-completion-list-box-row.h"
#include "completion/ide-completion-list-box.h"
#include "completion/ide-completion-overlay.h"
#include "completion/ide-completion-proposal.h"
#include "completion/ide-completion-provider.h"
#include "completion/ide-completion-results.h"
#include "completion/ide-completion-window.h"
#include "config/ide-configuration.h"
#include "config/ide-configuration-manager.h"
#include "config/ide-configuration-provider.h"
#include "debugger/ide-debug-manager.h"
#include "debugger/ide-debugger-breakpoint.h"
#include "debugger/ide-debugger-breakpoints.h"
#include "debugger/ide-debugger-frame.h"
#include "debugger/ide-debugger-instruction.h"
#include "debugger/ide-debugger-library.h"
#include "debugger/ide-debugger-register.h"
#include "debugger/ide-debugger-thread-group.h"
#include "debugger/ide-debugger-thread.h"
#include "debugger/ide-debugger-types.h"
#include "debugger/ide-debugger-variable.h"
#include "debugger/ide-debugger.h"
#include "devices/ide-deploy-strategy.h"
#include "devices/ide-device-info.h"
#include "devices/ide-device-manager.h"
#include "devices/ide-device-provider.h"
#include "devices/ide-device.h"
#include "diagnostics/ide-diagnostic-provider.h"
#include "diagnostics/ide-diagnostic.h"
#include "diagnostics/ide-diagnostics-manager.h"
#include "diagnostics/ide-diagnostics.h"
#include "diagnostics/ide-source-location.h"
#include "diagnostics/ide-source-range.h"
#include "doap/ide-doap-person.h"
#include "doap/ide-doap.h"
#include "documentation/ide-documentation.h"
#include "documentation/ide-documentation-info.h"
#include "documentation/ide-documentation-proposal.h"
#include "documentation/ide-documentation-provider.h"
#include "editor/ide-editor-addin.h"
#include "editor/ide-editor-perspective.h"
#include "editor/ide-editor-search.h"
#include "editor/ide-editor-sidebar.h"
#include "editor/ide-editor-utilities.h"
#include "editor/ide-editor-view-addin.h"
#include "editor/ide-editor-view.h"
#include "files/ide-file-settings.h"
#include "files/ide-file.h"
#include "files/ide-indent-style.h"
#include "files/ide-spaces-style.h"
#include "genesis/ide-genesis-addin.h"
#include "greeter/ide-greeter-section.h"
#include "highlighting/ide-highlight-engine.h"
#include "highlighting/ide-highlight-index.h"
#include "highlighting/ide-highlighter.h"
#include "langserv/ide-langserv-client.h"
#include "langserv/ide-langserv-completion-item.h"
#include "langserv/ide-langserv-completion-provider.h"
#include "langserv/ide-langserv-completion-results.h"
#include "langserv/ide-langserv-diagnostic-provider.h"
#include "langserv/ide-langserv-rename-provider.h"
#include "langserv/ide-langserv-symbol-resolver.h"
#include "langserv/ide-langserv-types.h"
#include "langserv/ide-langserv-util.h"
#include "layout/ide-layout-grid.h"
#include "layout/ide-layout-grid-column.h"
#include "layout/ide-layout-pane.h"
#include "layout/ide-layout-stack-addin.h"
#include "layout/ide-layout-stack-header.h"
#include "layout/ide-layout-stack.h"
#include "layout/ide-layout-transient-sidebar.h"
#include "layout/ide-layout-view.h"
#include "layout/ide-layout.h"
#include "local/ide-local-device.h"
#include "logging/ide-log.h"
#include "preferences/ide-preferences-addin.h"
#include "preferences/ide-preferences-perspective.h"
#include "preferences/ide-preferences-window.h"
#include "projects/ide-project-edit.h"
#include "projects/ide-project-info.h"
#include "projects/ide-project-item.h"
#include "projects/ide-project.h"
#include "projects/ide-recent-projects.h"
#include "rename/ide-rename-provider.h"
#include "runner/ide-run-manager.h"
#include "runner/ide-runner.h"
#include "runner/ide-runner-addin.h"
#include "runtimes/ide-runtime-manager.h"
#include "runtimes/ide-runtime-provider.h"
#include "runtimes/ide-runtime.h"
#include "search/ide-search-engine.h"
#include "search/ide-search-entry.h"
#include "search/ide-search-provider.h"
#include "search/ide-search-reducer.h"
#include "search/ide-search-result.h"
#include "search/ide-tagged-entry.h"
#include "snippets/ide-snippet.h"
#include "snippets/ide-snippet-chunk.h"
#include "snippets/ide-snippet-context.h"
#include "snippets/ide-snippet-parser.h"
#include "snippets/ide-snippet-storage.h"
#include "sourceview/ide-indenter.h"
#include "sourceview/ide-language.h"
#include "sourceview/ide-source-map.h"
#include "sourceview/ide-source-style-scheme.h"
#include "sourceview/ide-source-view.h"
#include "storage/ide-persistent-map.h"
#include "storage/ide-persistent-map-builder.h"
#include "subprocess/ide-subprocess.h"
#include "subprocess/ide-subprocess-launcher.h"
#include "subprocess/ide-subprocess-supervisor.h"
#include "symbols/ide-code-indexer.h"
#include "symbols/ide-code-index-entry.h"
#include "symbols/ide-symbol-resolver.h"
#include "symbols/ide-symbol.h"
#include "symbols/ide-tags-builder.h"
#include "template/ide-project-template.h"
#include "template/ide-template-provider.h"
#include "testing/ide-test.h"
#include "testing/ide-test-manager.h"
#include "testing/ide-test-provider.h"
#include "threading/ide-task.h"
#include "threading/ide-thread-pool.h"
#include "toolchain/ide-simple-toolchain.h"
#include "toolchain/ide-toolchain.h"
#include "toolchain/ide-toolchain-manager.h"
#include "toolchain/ide-toolchain-provider.h"
#include "terminal/ide-terminal.h"
#include "terminal/ide-terminal-search.h"
#include "terminal/ide-terminal-util.h"
#include "transfers/ide-pkcon-transfer.h"
#include "transfers/ide-transfer.h"
#include "transfers/ide-transfer-button.h"
#include "transfers/ide-transfer-manager.h"
#include "util/ide-cell-renderer-fancy.h"
#include "util/ide-fancy-tree-view.h"
#include "util/ide-flatpak.h"
#include "util/ide-glib.h"
#include "util/ide-gtk.h"
#include "util/ide-line-reader.h"
#include "util/ide-list-inline.h"
#include "util/ide-posix.h"
#include "util/ide-progress.h"
#include "util/ide-ref-ptr.h"
#include "util/ide-settings.h"
#include "util/ide-triplet.h"
#include "util/ide-uri.h"
#include "vcs/ide-vcs-config.h"
#include "vcs/ide-vcs-file-info.h"
#include "vcs/ide-vcs-initializer.h"
#include "vcs/ide-vcs-monitor.h"
#include "vcs/ide-vcs-uri.h"
#include "vcs/ide-vcs.h"
#include "workbench/ide-perspective.h"
#include "workbench/ide-workbench-addin.h"
#include "workbench/ide-workbench-message.h"
#include "workbench/ide-workbench-header-bar.h"
#include "workbench/ide-workbench.h"

#undef IDE_INSIDE

G_END_DECLS
