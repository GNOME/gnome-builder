/* ide-buffer.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-buffer"

#include <glib/gi18n.h>

#include "ide-buffer.h"
#include "ide-context.h"
#include "ide-diagnostic.h"
#include "ide-diagnostician.h"
#include "ide-diagnostics.h"
#include "ide-file.h"
#include "ide-file-settings.h"
#include "ide-language.h"
#include "ide-source-location.h"
#include "ide-source-range.h"
#include "ide-unsaved-files.h"

#define DEFAULT_DIAGNOSE_TIMEOUT_MSEC 333

#define TAG_ERROR   "diagnostician::error"
#define TAG_WARNING "diagnostician::warning"
#define TAG_NOTE    "diagnostician::note"

typedef struct _IdeBufferClass
{
  GtkSourceBufferClass parent;
} IdeBufferClass;

struct _IdeBuffer
{
  GtkSourceBuffer  parent_instance;

  IdeContext      *context;
  IdeDiagnostics  *diagnostics;
  GHashTable      *diagnostics_line_cache;
  IdeFile         *file;

  guint            diagnose_timeout;

  guint            diagnostics_dirty : 1;
  guint            in_diagnose : 1;
  guint            highlight_diagnostics : 1;
};

G_DEFINE_TYPE (IdeBuffer, ide_buffer, GTK_SOURCE_TYPE_BUFFER)

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_FILE,
  PROP_HIGHLIGHT_DIAGNOSTICS,
  LAST_PROP
};

static void ide_buffer_queue_diagnose (IdeBuffer *self);

static GParamSpec *gParamSpecs [LAST_PROP];

static void
ide_buffer_get_iter_at_location (IdeBuffer         *self,
                                 GtkTextIter       *iter,
                                 IdeSourceLocation *location)
{
  guint line;
  guint line_offset;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (iter);
  g_assert (location);

  line = ide_source_location_get_line (location);
  line_offset = ide_source_location_get_line_offset (location);

  gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (self), iter, line);

  while (line_offset && !gtk_text_iter_ends_line (iter))
    {
      gtk_text_iter_forward_char (iter);
      line_offset--;
    }
}

static void
ide_buffer_set_context (IdeBuffer  *self,
                        IdeContext *context)
{
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (self->context == NULL);

  ide_set_weak_pointer (&self->context, context);
}

static void
ide_buffer_sync_to_unsaved_files (IdeBuffer *self)
{
  IdeUnsavedFiles *unsaved_files;
  GtkTextBuffer *buffer = (GtkTextBuffer *)self;
  gchar *text;
  GFile *gfile;
  GBytes *content;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_BUFFER (self));

  if (!self->context || !self->file)
    return;

  gfile = ide_file_get_file (self->file);
  if (!gfile)
    return;

  gtk_text_buffer_get_bounds (buffer, &begin, &end);
  text = gtk_text_buffer_get_text (buffer, &begin, &end, TRUE);
  content = g_bytes_new_take (text, strlen (text));

  unsaved_files = ide_context_get_unsaved_files (self->context);
  ide_unsaved_files_update (unsaved_files, gfile, content);

  g_bytes_unref (content);
}

static void
ide_buffer_clear_diagnostics (IdeBuffer *self)
{
  GtkTextBuffer *buffer = (GtkTextBuffer *)self;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_BUFFER (self));

  if (self->diagnostics_line_cache)
    g_hash_table_remove_all (self->diagnostics_line_cache);

  gtk_text_buffer_get_bounds (buffer, &begin, &end);

  gtk_text_buffer_remove_tag_by_name (buffer, TAG_NOTE, &begin, &end);
  gtk_text_buffer_remove_tag_by_name (buffer, TAG_WARNING, &begin, &end);
  gtk_text_buffer_remove_tag_by_name (buffer, TAG_ERROR, &begin, &end);
}

static void
ide_buffer_cache_diagnostic_line (IdeBuffer             *self,
                                  IdeSourceLocation     *begin,
                                  IdeSourceLocation     *end,
                                  IdeDiagnosticSeverity  severity)
{
  gpointer new_value = GINT_TO_POINTER (severity);
  gsize line_begin;
  gsize line_end;
  gsize i;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (begin);
  g_assert (end);

  if (!self->diagnostics_line_cache)
    return;

  line_begin = MIN (ide_source_location_get_line (begin),
                    ide_source_location_get_line (end));
  line_end = MAX (ide_source_location_get_line (begin),
                  ide_source_location_get_line (end));

  for (i = line_begin; i <= line_end; i++)
    {
      gpointer old_value;
      gpointer key = GINT_TO_POINTER (i);

      old_value = g_hash_table_lookup (self->diagnostics_line_cache, key);

      if (new_value > old_value)
        g_hash_table_replace (self->diagnostics_line_cache, key, new_value);
    }
}

static void
ide_buffer_update_diagnostic (IdeBuffer     *self,
                              IdeDiagnostic *diagnostic)
{
  IdeDiagnosticSeverity severity;
  const gchar *tag_name = NULL;
  IdeSourceLocation *location;
  gsize num_ranges;
  gsize i;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (diagnostic);

  severity = ide_diagnostic_get_severity (diagnostic);

  switch (severity)
    {
    case IDE_DIAGNOSTIC_NOTE:
      tag_name = TAG_NOTE;
      break;

    case IDE_DIAGNOSTIC_WARNING:
      tag_name = TAG_WARNING;
      break;

    case IDE_DIAGNOSTIC_ERROR:
    case IDE_DIAGNOSTIC_FATAL:
      tag_name = TAG_ERROR;
      break;

    case IDE_DIAGNOSTIC_IGNORED:
    default:
      return;
    }

  if ((location = ide_diagnostic_get_location (diagnostic)))
    {
      IdeFile *file;
      GtkTextIter iter1;
      GtkTextIter iter2;

      file = ide_source_location_get_file (location);

      if (file && self->file && !ide_file_equal (file, self->file))
        {
          /* Ignore? */
        }

      ide_buffer_cache_diagnostic_line (self, location, location, severity);

      ide_buffer_get_iter_at_location (self, &iter1, location);
      gtk_text_iter_assign (&iter2, &iter1);
      if (!gtk_text_iter_ends_line (&iter2))
        gtk_text_iter_forward_to_line_end (&iter2);
      else
        gtk_text_iter_backward_char (&iter1);

      gtk_text_buffer_apply_tag_by_name (GTK_TEXT_BUFFER (self), tag_name, &iter1, &iter2);
    }

  num_ranges = ide_diagnostic_get_num_ranges (diagnostic);

  for (i = 0; i < num_ranges; i++)
    {
      IdeSourceRange *range;
      IdeSourceLocation *begin;
      IdeSourceLocation *end;
      IdeFile *file;
      GtkTextIter iter1;
      GtkTextIter iter2;

      range = ide_diagnostic_get_range (diagnostic, i);
      begin = ide_source_range_get_begin (range);
      end = ide_source_range_get_end (range);

      file = ide_source_location_get_file (begin);

      if (file && self->file && !ide_file_equal (file, self->file))
        {
          /* Ignore */
        }

      ide_buffer_get_iter_at_location (self, &iter1, begin);
      ide_buffer_get_iter_at_location (self, &iter2, end);

      ide_buffer_cache_diagnostic_line (self, begin, end, severity);

      if (gtk_text_iter_equal (&iter1, &iter2))
        {
          if (!gtk_text_iter_ends_line (&iter2))
            gtk_text_iter_forward_char (&iter2);
          else
            gtk_text_iter_backward_char (&iter1);
        }

      gtk_text_buffer_apply_tag_by_name (GTK_TEXT_BUFFER (self), tag_name, &iter1, &iter2);
    }
}

static void
ide_buffer_update_diagnostics (IdeBuffer      *self,
                               IdeDiagnostics *diagnostics)
{
  gsize size;
  gsize i;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (diagnostics);

  size = ide_diagnostics_get_size (diagnostics);

  for (i = 0; i < size; i++)
    {
      IdeDiagnostic *diagnostic;

      diagnostic = ide_diagnostics_index (diagnostics, i);
      ide_buffer_update_diagnostic (self, diagnostic);
    }
}

static void
ide_buffer_set_diagnostics (IdeBuffer      *self,
                            IdeDiagnostics *diagnostics)
{
  g_assert (IDE_IS_BUFFER (self));

  if (diagnostics != self->diagnostics)
    {
      g_clear_pointer (&self->diagnostics, ide_diagnostics_unref);
      self->diagnostics = diagnostics ? ide_diagnostics_ref (diagnostics) : NULL;

      ide_buffer_clear_diagnostics (self);

      if (diagnostics)
        ide_buffer_update_diagnostics (self, diagnostics);
    }
}

static void
ide_buffer__file_load_settings_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  g_autoptr(IdeBuffer) self = user_data;
  IdeFile *file = (IdeFile *)object;
  g_autoptr(IdeFileSettings) file_settings = NULL;

  g_assert (IDE_IS_BUFFER (self));
  g_assert (IDE_IS_FILE (file));

  file_settings = ide_file_load_settings_finish (file, result, NULL);

  if (file_settings)
    {
      gboolean insert_trailing_newline;

      insert_trailing_newline = ide_file_settings_get_insert_trailing_newline (file_settings);
      gtk_source_buffer_set_implicit_trailing_newline (GTK_SOURCE_BUFFER (self),
                                                       insert_trailing_newline);
    }
}

static void
ide_buffer__diagnostician_diagnose_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeDiagnostician *diagnostician = (IdeDiagnostician *)object;
  g_autoptr(IdeBuffer) self = user_data;
  g_autoptr(IdeDiagnostics) diagnostics = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_DIAGNOSTICIAN (diagnostician));
  g_assert (IDE_IS_BUFFER (self));

  self->in_diagnose = FALSE;

  diagnostics = ide_diagnostician_diagnose_finish (diagnostician, result, &error);

  if (error)
    g_message ("%s", error->message);

  ide_buffer_set_diagnostics (self, diagnostics);

  if (self->diagnostics_dirty)
    ide_buffer_queue_diagnose (self);
}

static gboolean
ide_buffer__diagnose_timeout_cb (gpointer user_data)
{
  IdeBuffer *self = user_data;

  g_assert (IDE_IS_BUFFER (self));

  self->diagnose_timeout = 0;

  if (self->file)
    {
      IdeLanguage *language;

      language = ide_file_get_language (self->file);

      if (language)
        {
          IdeDiagnostician *diagnostician;

          diagnostician = ide_language_get_diagnostician (language);

          if (diagnostician)
            {
              self->diagnostics_dirty = FALSE;
              self->in_diagnose = TRUE;

              ide_buffer_sync_to_unsaved_files (self);
              ide_diagnostician_diagnose_async (diagnostician, self->file, NULL,
                                                ide_buffer__diagnostician_diagnose_cb,
                                                g_object_ref (self));
            }
        }
    }

  return G_SOURCE_REMOVE;
}

static void
ide_buffer_queue_diagnose (IdeBuffer *self)
{
  g_assert (IDE_IS_BUFFER (self));

  self->diagnostics_dirty = TRUE;

  if (self->diagnose_timeout != 0)
    {
      g_source_remove (self->diagnose_timeout);
      self->diagnose_timeout = 0;
    }

  self->diagnose_timeout = g_timeout_add (DEFAULT_DIAGNOSE_TIMEOUT_MSEC,
                                          ide_buffer__diagnose_timeout_cb,
                                          self);
}

static void
ide_buffer_changed (GtkTextBuffer *buffer)
{
  IdeBuffer *self = (IdeBuffer *)buffer;

  GTK_TEXT_BUFFER_CLASS (ide_buffer_parent_class)->changed (buffer);

  self->diagnostics_dirty = TRUE;

  if (self->highlight_diagnostics && !self->in_diagnose)
    ide_buffer_queue_diagnose (self);
}

static void
ide_buffer_constructed (GObject *object)
{
  IdeBuffer *self = (IdeBuffer *)object;

  G_OBJECT_CLASS (ide_buffer_parent_class)->constructed (object);

  gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self), TAG_ERROR,
                              "underline", PANGO_UNDERLINE_ERROR,
                              NULL);
  gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self), TAG_WARNING,
                              "underline", PANGO_UNDERLINE_ERROR,
                              NULL);
  gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (self), TAG_NOTE,
                              "underline", PANGO_UNDERLINE_SINGLE,
                              NULL);
}

static void
ide_buffer_dispose (GObject *object)
{
  IdeBuffer *self = (IdeBuffer *)object;

  if (self->diagnose_timeout)
    {
      g_source_remove (self->diagnose_timeout);
      self->diagnose_timeout = 0;
    }

  g_clear_pointer (&self->diagnostics_line_cache, g_hash_table_unref);
  g_clear_pointer (&self->diagnostics, ide_diagnostics_unref);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (ide_buffer_parent_class)->dispose (object);
}

static void
ide_buffer_finalize (GObject *object)
{
  IdeBuffer *self = (IdeBuffer *)object;

  ide_clear_weak_pointer (&self->context);

  G_OBJECT_CLASS (ide_buffer_parent_class)->finalize (object);
}

static void
ide_buffer_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  IdeBuffer *self = IDE_BUFFER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, ide_buffer_get_context (self));
      break;

    case PROP_FILE:
      g_value_set_object (value, ide_buffer_get_file (self));
      break;

    case PROP_HIGHLIGHT_DIAGNOSTICS:
      g_value_set_boolean (value, ide_buffer_get_highlight_diagnostics (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_buffer_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  IdeBuffer *self = IDE_BUFFER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      ide_buffer_set_context (self, g_value_get_object (value));
      break;

    case PROP_FILE:
      ide_buffer_set_file (self, g_value_get_object (value));
      break;

    case PROP_HIGHLIGHT_DIAGNOSTICS:
      ide_buffer_set_highlight_diagnostics (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_buffer_class_init (IdeBufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkTextBufferClass *text_buffer_class = GTK_TEXT_BUFFER_CLASS (klass);

  object_class->constructed = ide_buffer_constructed;
  object_class->dispose = ide_buffer_dispose;
  object_class->finalize = ide_buffer_finalize;
  object_class->get_property = ide_buffer_get_property;
  object_class->set_property = ide_buffer_set_property;

  text_buffer_class->changed = ide_buffer_changed;

  gParamSpecs [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         _("Context"),
                         _("The IdeContext for the buffer."),
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CONTEXT,
                                   gParamSpecs [PROP_CONTEXT]);

  gParamSpecs [PROP_FILE] =
    g_param_spec_object ("file",
                         _("File"),
                         _("The file represented by the buffer."),
                         IDE_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FILE, gParamSpecs [PROP_FILE]);

  gParamSpecs [PROP_HIGHLIGHT_DIAGNOSTICS] =
    g_param_spec_boolean ("highlight-diagnostics",
                          _("Highlight Diagnostics"),
                          _("If diagnostic warnings and errors should be highlighted."),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_HIGHLIGHT_DIAGNOSTICS,
                                   gParamSpecs [PROP_HIGHLIGHT_DIAGNOSTICS]);
}

static void
ide_buffer_init (IdeBuffer *self)
{
  self->diagnostics_line_cache = g_hash_table_new (g_direct_hash, g_direct_equal);
}

GType
ide_buffer_line_flags_get_type (void)
{
  static gsize type_id;

  if (g_once_init_enter (&type_id))
    {
      GType _type_id;
      const static GFlagsValue values[] = {
        { IDE_BUFFER_LINE_FLAGS_NONE, "IDE_BUFFER_LINE_FLAGS_NONE", "NONE" },
        { IDE_BUFFER_LINE_FLAGS_ADDED, "IDE_BUFFER_LINE_FLAGS_ADDED", "ADDED" },
        { IDE_BUFFER_LINE_FLAGS_CHANGED, "IDE_BUFFER_LINE_FLAGS_CHANGED", "CHANGED" },
        { IDE_BUFFER_LINE_FLAGS_ERROR, "IDE_BUFFER_LINE_FLAGS_ERROR", "ERROR" },
        { IDE_BUFFER_LINE_FLAGS_WARNING, "IDE_BUFFER_LINE_FLAGS_WARNING", "WARNING" },
        { IDE_BUFFER_LINE_FLAGS_NOTE, "IDE_BUFFER_LINE_FLAGS_NOTE", "NOTE" },
        { 0 }
      };

      _type_id = g_flags_register_static ("IdeBufferLineFlags", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}

/**
 * ide_buffer_get_file:
 *
 * Gets the underlying file behind the buffer.
 *
 * Returns: (transfer none): An #IdeFile.
 */
IdeFile *
ide_buffer_get_file (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return self->file;
}

/**
 * ide_buffer_set_file:
 *
 * Sets the underlying file to use when saving and loading @self to and and from storage.
 */
void
ide_buffer_set_file (IdeBuffer *self,
                     IdeFile   *file)
{
  g_return_if_fail (IDE_IS_BUFFER (self));
  g_return_if_fail (IDE_IS_FILE (file));

  if (g_set_object (&self->file, file))
    {
      ide_file_load_settings_async (self->file,
                                    NULL,
                                    ide_buffer__file_load_settings_cb,
                                    g_object_ref (self));

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_FILE]);
    }
}

/**
 * ide_buffer_get_context:
 *
 * Gets the #IdeBuffer:context property. This is the #IdeContext that owns the buffer.
 *
 * Returns: (transfer none): An #IdeContext.
 */
IdeContext *
ide_buffer_get_context (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);

  return self->context;
}

IdeBufferLineFlags
ide_buffer_get_line_flags (IdeBuffer *self,
                           guint      line)
{
  IdeBufferLineFlags flags = 0;

  if (self->diagnostics_line_cache)
    {
      gpointer key = GINT_TO_POINTER (line);
      gpointer value;

      value = g_hash_table_lookup (self->diagnostics_line_cache, key);

      switch (GPOINTER_TO_INT (value))
        {
        case IDE_DIAGNOSTIC_FATAL:
        case IDE_DIAGNOSTIC_ERROR:
          flags |= IDE_BUFFER_LINE_FLAGS_ERROR;
          break;

        case IDE_DIAGNOSTIC_WARNING:
          flags |= IDE_BUFFER_LINE_FLAGS_WARNING;
          break;

        case IDE_DIAGNOSTIC_NOTE:
          flags |= IDE_BUFFER_LINE_FLAGS_NOTE;
          break;

        default:
          break;
        }
    }

  /* TODO: Coordinate with Vcs */
  flags |= IDE_BUFFER_LINE_FLAGS_ADDED;

  return flags;
}

gboolean
ide_buffer_get_highlight_diagnostics (IdeBuffer *self)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), FALSE);

  return self->highlight_diagnostics;
}

void
ide_buffer_set_highlight_diagnostics (IdeBuffer *self,
                                      gboolean   highlight_diagnostics)
{
  g_return_if_fail (IDE_IS_BUFFER (self));

  highlight_diagnostics = !!highlight_diagnostics;

  if (highlight_diagnostics != self->highlight_diagnostics)
    {
      self->highlight_diagnostics = highlight_diagnostics;
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_HIGHLIGHT_DIAGNOSTICS]);
    }
}

/**
 * ide_buffer_get_diagnostic_at_iter:
 *
 * Gets the first diagnostic that overlaps the position
 *
 * Returns: (transfer none) (nullable): An #IdeDiagnostic or %NULL.
 */
IdeDiagnostic *
ide_buffer_get_diagnostic_at_iter (IdeBuffer         *self,
                                   const GtkTextIter *iter)
{
  g_return_val_if_fail (IDE_IS_BUFFER (self), NULL);
  g_return_val_if_fail (iter, NULL);

  if (self->diagnostics)
    {
      IdeDiagnostic *diagnostic = NULL;
      IdeBufferLineFlags flags;
      guint distance = G_MAXUINT;
      gsize size;
      gsize i;
      guint line;

      line = gtk_text_iter_get_line (iter);
      flags = ide_buffer_get_line_flags (self, line);

      if ((flags & IDE_BUFFER_LINE_FLAGS_DIAGNOSTICS_MASK) == 0)
        return NULL;

      size = ide_diagnostics_get_size (self->diagnostics);

      for (i = 0; i < size; i++)
        {
          IdeDiagnostic *diag;
          IdeSourceLocation *location;
          GtkTextIter pos;

          diag = ide_diagnostics_index (self->diagnostics, i);
          location = ide_diagnostic_get_location (diag);
          if (!location)
            continue;

          ide_buffer_get_iter_at_location (self, &pos, location);

          if (line == gtk_text_iter_get_line (&pos))
            {
              guint offset;

              offset = ABS (gtk_text_iter_get_offset (iter) - gtk_text_iter_get_offset (&pos));

              if (offset < distance)
                {
                  distance = offset;
                  diagnostic = diag;
                }
            }
        }

      return diagnostic;
    }

  return NULL;
}
