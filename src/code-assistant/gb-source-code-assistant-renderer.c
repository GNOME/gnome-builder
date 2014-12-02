/* gb-source-code-assistant-renderer.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "code-assist-gutter"

#include <glib/gi18n.h>

#include "gb-log.h"
#include "gb-source-code-assistant.h"
#include "gb-source-code-assistant-renderer.h"
#include "gca-structs.h"

struct _GbSourceCodeAssistantRendererPrivate
{
  GbSourceCodeAssistant *code_assistant;
  GHashTable            *line_to_severity_hash;
  GArray                *diagnostics;
  gulong                 changed_handler;
};

enum
{
  PROP_0,
  PROP_CODE_ASSISTANT,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceCodeAssistantRenderer,
                            gb_source_code_assistant_renderer,
                            GTK_SOURCE_TYPE_GUTTER_RENDERER_PIXBUF)

static GParamSpec *gParamSpecs [LAST_PROP];

GbSourceCodeAssistant *
gb_source_code_assistant_renderer_get_code_assistant (GbSourceCodeAssistantRenderer *renderer)
{
  g_return_val_if_fail (GB_IS_SOURCE_CODE_ASSISTANT_RENDERER (renderer), NULL);

  return renderer->priv->code_assistant;
}

static void
gb_source_code_assistant_renderer_add_diagnostic_range (GbSourceCodeAssistantRenderer *renderer,
                                                        GcaDiagnostic                 *diag,
                                                        GcaSourceRange                *range)
{
  gint64 i;

  g_assert (GB_IS_SOURCE_CODE_ASSISTANT_RENDERER (renderer));
  g_assert (diag);
  g_assert (range);

  if (range->begin.line == -1 || range->end.line == -1)
    return;

  g_return_if_fail (renderer->priv->line_to_severity_hash);

  for (i = range->begin.line; i <= range->end.line; i++)
    {
      gpointer val;

      val = g_hash_table_lookup (renderer->priv->line_to_severity_hash,
                                 GINT_TO_POINTER (i));
      if (GPOINTER_TO_INT (val) < diag->severity)
        val = GINT_TO_POINTER (diag->severity);

      g_hash_table_replace (renderer->priv->line_to_severity_hash,
                            GINT_TO_POINTER (i), val);
    }
}

static void
gb_source_code_assistant_renderer_changed (GbSourceCodeAssistantRenderer *renderer,
                                           GbSourceCodeAssistant         *code_assistant)
{
  GbSourceCodeAssistantRendererPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT_RENDERER (renderer));
  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT (code_assistant));

  priv = renderer->priv;

  g_hash_table_remove_all (priv->line_to_severity_hash);

  if (priv->diagnostics)
    {
      g_array_unref (priv->diagnostics);
      priv->diagnostics = NULL;
    }

  priv->diagnostics = gb_source_code_assistant_get_diagnostics (code_assistant);

  if (priv->diagnostics)
    {
      guint i;

      for (i = 0; i < priv->diagnostics->len; i++)
        {
          GcaDiagnostic *diag;

          diag = &g_array_index (priv->diagnostics, GcaDiagnostic, i);

          if (diag->locations)
            {
              guint j;

              for (j = 0; j < diag->locations->len; j++)
                {
                  GcaSourceRange *range;

                  range = &g_array_index (diag->locations, GcaSourceRange, j);
                  gb_source_code_assistant_renderer_add_diagnostic_range (renderer, diag, range);
                }
            }
        }
    }

  gtk_source_gutter_renderer_queue_draw (GTK_SOURCE_GUTTER_RENDERER (renderer));
}

static void
gb_source_code_assistant_renderer_connect (GbSourceCodeAssistantRenderer *renderer)
{
  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT_RENDERER (renderer));

  renderer->priv->changed_handler =
    g_signal_connect_object (renderer->priv->code_assistant,
                             "changed",
                             G_CALLBACK (gb_source_code_assistant_renderer_changed),
                             renderer,
                             G_CONNECT_SWAPPED);
}

static void
gb_source_code_assistant_renderer_disconnect (GbSourceCodeAssistantRenderer *renderer)
{
  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT_RENDERER (renderer));

  g_signal_handler_disconnect (renderer->priv->code_assistant,
                               renderer->priv->changed_handler);
  renderer->priv->changed_handler = 0;
}

void
gb_source_code_assistant_renderer_set_code_assistant (GbSourceCodeAssistantRenderer *renderer,
                                                      GbSourceCodeAssistant         *code_assistant)
{
  GbSourceCodeAssistantRendererPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT_RENDERER (renderer));
  g_return_if_fail (!code_assistant || GB_IS_SOURCE_CODE_ASSISTANT (code_assistant));

  priv = renderer->priv;

  if (code_assistant != priv->code_assistant)
    {
      if (priv->code_assistant)
        {
          gb_source_code_assistant_renderer_disconnect (renderer);
          g_object_remove_weak_pointer (G_OBJECT (priv->code_assistant),
                                        (gpointer *)&priv->code_assistant);
          priv->code_assistant = NULL;
        }

      if (code_assistant)
        {
          priv->code_assistant = code_assistant;
          g_object_add_weak_pointer (G_OBJECT (priv->code_assistant),
                                     (gpointer *)&priv->code_assistant);
          gb_source_code_assistant_renderer_connect (renderer);
        }

      gtk_source_gutter_renderer_queue_draw (GTK_SOURCE_GUTTER_RENDERER (renderer));

      g_object_notify_by_pspec (G_OBJECT (renderer),
                                gParamSpecs [PROP_CODE_ASSISTANT]);
    }
}

static void
gb_source_code_assistant_renderer_query_data (GtkSourceGutterRenderer      *renderer,
                                              GtkTextIter                  *begin,
                                              GtkTextIter                  *end,
                                              GtkSourceGutterRendererState  state)
{
  GbSourceCodeAssistantRenderer *self = (GbSourceCodeAssistantRenderer *)renderer;
  const gchar *icon_name = NULL;
  gpointer key;
  gpointer val;
  guint line;

  g_return_if_fail (GB_IS_SOURCE_CODE_ASSISTANT_RENDERER (self));

  line = gtk_text_iter_get_line (begin);

  key = GINT_TO_POINTER (line);
  val = g_hash_table_lookup (self->priv->line_to_severity_hash, key);

  switch (GPOINTER_TO_INT (val))
    {
    case GCA_SEVERITY_FATAL:
    case GCA_SEVERITY_ERROR:
      icon_name = "process-stop-symbolic";
      break;

    case GCA_SEVERITY_INFO:
      icon_name = "dialog-information-symbolic";
      break;

    case GCA_SEVERITY_DEPRECATED:
    case GCA_SEVERITY_WARNING:
      icon_name = "dialog-warning-symbolic";
      break;

    case GCA_SEVERITY_NONE:
    default:
      break;
    }

  if (icon_name)
    g_object_set (renderer, "icon-name", icon_name, NULL);
  else
    g_object_set (renderer, "pixbuf", NULL, NULL);
}

static void
gb_source_code_assistant_renderer_finalize (GObject *object)
{
  GbSourceCodeAssistantRendererPrivate *priv;

  ENTRY;

  priv = GB_SOURCE_CODE_ASSISTANT_RENDERER (object)->priv;

  if (priv->code_assistant)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->code_assistant),
                                    (gpointer *)&priv->code_assistant);
      priv->code_assistant = NULL;
    }

  if (priv->diagnostics)
    g_clear_pointer (&priv->diagnostics, g_array_unref);

  g_clear_pointer (&priv->line_to_severity_hash, g_hash_table_unref);

  G_OBJECT_CLASS (gb_source_code_assistant_renderer_parent_class)->finalize (object);

  EXIT;
}

static void
gb_source_code_assistant_renderer_get_property (GObject    *object,
                                                guint       prop_id,
                                                GValue     *value,
                                                GParamSpec *pspec)
{
  GbSourceCodeAssistantRenderer *self = GB_SOURCE_CODE_ASSISTANT_RENDERER (object);

  switch (prop_id)
    {
    case PROP_CODE_ASSISTANT:
      g_value_set_object (value, gb_source_code_assistant_renderer_get_code_assistant (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_code_assistant_renderer_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
  GbSourceCodeAssistantRenderer *self = GB_SOURCE_CODE_ASSISTANT_RENDERER (object);

  switch (prop_id)
    {
    case PROP_CODE_ASSISTANT:
      gb_source_code_assistant_renderer_set_code_assistant (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_code_assistant_renderer_class_init (GbSourceCodeAssistantRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);

  object_class->finalize = gb_source_code_assistant_renderer_finalize;
  object_class->get_property = gb_source_code_assistant_renderer_get_property;
  object_class->set_property = gb_source_code_assistant_renderer_set_property;

  renderer_class->query_data = gb_source_code_assistant_renderer_query_data;

  gParamSpecs [PROP_CODE_ASSISTANT] =
    g_param_spec_object ("code-assistant",
                         _("Code Assistant"),
                         _("The code assistant to render."),
                         GB_TYPE_SOURCE_CODE_ASSISTANT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CODE_ASSISTANT,
                                   gParamSpecs [PROP_CODE_ASSISTANT]);
}

static void
gb_source_code_assistant_renderer_init (GbSourceCodeAssistantRenderer *renderer)
{
  renderer->priv = gb_source_code_assistant_renderer_get_instance_private (renderer);
  renderer->priv->line_to_severity_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
}
