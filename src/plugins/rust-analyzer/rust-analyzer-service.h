/* rust-analyzer-service.h
 *
 * Copyright 2020 Günther Wagner <info@gunibert.de>
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-lsp.h>

G_BEGIN_DECLS

#define RUST_TYPE_ANALYZER_SERVICE (rust_analyzer_service_get_type())

G_DECLARE_FINAL_TYPE (RustAnalyzerService, rust_analyzer_service, RUST, ANALYZER_SERVICE, GObject)

RustAnalyzerService *rust_analyzer_service_from_context   (IdeContext          *context);
IdeLspClient        *rust_analyzer_service_get_client     (RustAnalyzerService *self);
void                 rust_analyzer_service_ensure_started (RustAnalyzerService *self);

G_END_DECLS
