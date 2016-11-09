## Buffer Manager

Libide has a buffer manager per-project context (each context has its own
workbench window). Buffers exist within the buffer manager, and can be
autosaved when it is appropriate.

## Buffer

This is our actual buffer. It is a GtkTextBuffer (and GtkSourceBuffer) with
a bunch of extra smarts to help us interact with version control, diagnostics,
semantic highlighters, and more. You connect one of these to an IdeSourceView.

## Unsaved Files

This manages a collection of unsaved files. We often need to pass buffers off
to the compiler subsystem, and anything we can do to avoid calling
gtk_text_buffer_get_text() a bunch helps. This allows us to reduce the number
of calls we do on that.

## Unsaved File

The unsaved file instance represents a single file within the Unsaved Files.
This might get persisted to a drafts directory periodically. It can also be
passed to subsystems that might need access to the whole, raw, buffer, such
as the clang compiler.

## Buffer Change Monitor

The Buffer Change Monitor helps track changes to the buffer such as added
and deleted lines. This can be rendered in the gutter of a source view.
