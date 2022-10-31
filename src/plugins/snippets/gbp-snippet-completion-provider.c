/* gbp-snippet-completion-provider.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-snippet-completion-provider"

#include "config.h"

#include "gbp-snippet-completion-provider.h"

struct _GbpSnippetCompletionProvider
{
  GtkSourceCompletionSnippets parent_instance;
  guint enabled : 1;
};

enum {
  PROP_0,
  PROP_ENABLED,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];
static GtkSourceCompletionProviderInterface *parent_iface;

static GListModel *
gbp_snippet_completion_provider_populate (GtkSourceCompletionProvider  *provider,
                                          GtkSourceCompletionContext   *context,
                                          GError                      **error)
{
  GbpSnippetCompletionProvider *self = (GbpSnippetCompletionProvider *)provider;
  GtkTextIter begin, end;

  g_assert (GBP_IS_SNIPPET_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  if (!self->enabled)
    goto failure;

  if (gtk_source_completion_context_get_bounds (context, &begin, &end))
    {
      GtkSourceBuffer *buffer = gtk_source_completion_context_get_buffer (context);

      /* Don't suggest snippets while in strings or comments */
      if (gtk_source_buffer_iter_has_context_class (buffer, &begin, "comment") ||
          gtk_source_buffer_iter_has_context_class (buffer, &begin, "string"))
        goto failure;
    }

  return parent_iface->populate (provider, context, error);

failure:
  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               "Provider is disabled");
  return NULL;
}

static int
gbp_snippet_completion_provider_get_priority (GtkSourceCompletionProvider *provider,
                                              GtkSourceCompletionContext  *context)
{
  return 0;
}

static void
competion_provider_iface_init (GtkSourceCompletionProviderInterface *iface)
{
  parent_iface = g_type_interface_peek_parent (iface);
  iface->populate = gbp_snippet_completion_provider_populate;
  iface->get_priority = gbp_snippet_completion_provider_get_priority;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSnippetCompletionProvider, gbp_snippet_completion_provider, GTK_SOURCE_TYPE_COMPLETION_SNIPPETS,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, competion_provider_iface_init))

static void
gbp_snippet_completion_provider_constructed (GObject *object)
{
  GbpSnippetCompletionProvider *self = (GbpSnippetCompletionProvider *)object;
  static GSettings *editor_settings;

  G_OBJECT_CLASS (gbp_snippet_completion_provider_parent_class)->constructed (object);

  if (editor_settings == NULL)
    editor_settings = g_settings_new ("org.gnome.builder.editor");

  g_settings_bind (editor_settings, "enable-snippets",
                   self, "enabled",
                   G_SETTINGS_BIND_GET);
}

static void
gbp_snippet_completion_provider_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
  GbpSnippetCompletionProvider *self = GBP_SNIPPET_COMPLETION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, self->enabled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_snippet_completion_provider_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
  GbpSnippetCompletionProvider *self = GBP_SNIPPET_COMPLETION_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      self->enabled = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_snippet_completion_provider_class_init (GbpSnippetCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gbp_snippet_completion_provider_constructed;
  object_class->get_property = gbp_snippet_completion_provider_get_property;
  object_class->set_property = gbp_snippet_completion_provider_set_property;

  properties [PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          "Enabled",
                          "If the provider is enabled",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_snippet_completion_provider_init (GbpSnippetCompletionProvider *self)
{
}
