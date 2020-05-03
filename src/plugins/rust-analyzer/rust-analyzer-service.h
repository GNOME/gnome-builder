#pragma once

#include <libide-core.h>
#include <libide-lsp.h>

G_BEGIN_DECLS

#define RUST_TYPE_ANALYZER_SERVICE (rust_analyzer_service_get_type())

G_DECLARE_FINAL_TYPE (RustAnalyzerService, rust_analyzer_service, RUST, ANALYZER_SERVICE, IdeObject)

typedef enum {
  RUST_ANALYZER_SERVICE_INIT,
  RUST_ANALYZER_SERVICE_OFFER_DOWNLOAD,
  RUST_ANALYZER_SERVICE_READY,
  RUST_ANALYZER_SERVICE_LSP_STARTED,
} ServiceState;

RustAnalyzerService *rust_analyzer_service_new            (void);
IdeLspClient        *rust_analyzer_service_get_client     (RustAnalyzerService *self);
void                 rust_analyzer_service_set_client     (RustAnalyzerService *self,
                                                           IdeLspClient        *client);
void                 rust_analyzer_service_ensure_started (RustAnalyzerService *self);
void                 rust_analyzer_service_set_state      (RustAnalyzerService *self,
                                                           ServiceState         state);

G_END_DECLS
