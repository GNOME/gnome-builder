/* ide-word-completion-provider.c
 *
 * Copyright (C) 2017 Umang Jain <mailumangjain@gmail.com>
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

#define G_LOG_DOMAIN "ide-word-completion-provider"

#include <glib/gi18n.h>
#include <string.h>

#include "ide-debug.h"
#include "ide-macros.h"

#include "sourceview/ide-word-completion-provider.h"
#include "sourceview/ide-word-completion-item.h"
#include "sourceview/ide-word-completion-results.h"
#include "sourceview/ide-completion-provider.h"

typedef struct
{
  GtkSourceSearchContext        *search_context;
  GtkSourceSearchSettings       *search_settings;
  GtkSourceCompletionContext    *context;
  IdeWordCompletionResults      *results;
  GtkSourceCompletionActivation  activation;
  GHashTable                    *all_proposals;

  GIcon                         *icon;
  gchar                         *current_word;
  gulong                         cancel_id;
  gchar                         *name;
  gint                           interactive_delay;
  gint                           priority;
  gint                           direction;
  guint                          minimum_word_size;
  gboolean                       wrap_around_flag;

  /* No references, cleared in _finished_cb */
  GtkTextMark                    *start_mark;
  GtkTextMark                    *end_mark;
} IdeWordCompletionProviderPrivate;

struct _IdeWordCompletionProvider
{
  GObject parent;
};

static void ide_word_completion_provider_iface_init (GtkSourceCompletionProviderIface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeWordCompletionProvider,
                         ide_word_completion_provider,
                         G_TYPE_OBJECT,
                         G_ADD_PRIVATE (IdeWordCompletionProvider)
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, ide_word_completion_provider_iface_init)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, NULL))

enum
{
  PROP_0,
  PROP_NAME,
  PROP_ICON,
  PROP_INTERACTIVE_DELAY,
  PROP_PRIORITY,
  PROP_ACTIVATION,
  PROP_DIRECTION,
  PROP_MINIMUM_WORD_SIZE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

static gboolean
refresh_iters (IdeWordCompletionProvider *self,
               GtkTextIter               *match_start,
               GtkTextIter               *match_end)
{
  IdeWordCompletionProviderPrivate *priv = ide_word_completion_provider_get_instance_private (self);
  GtkTextBuffer *buffer = NULL;

  g_assert (IDE_IS_WORD_COMPLETION_PROVIDER (self));
  g_assert (priv->start_mark != NULL);
  g_assert (priv->end_mark != NULL);

  buffer = gtk_text_mark_get_buffer (priv->start_mark);

  if (buffer)
    {
      gtk_text_buffer_get_iter_at_mark (buffer, match_start, priv->start_mark);
      gtk_text_buffer_get_iter_at_mark (buffer, match_end, priv->end_mark);

      return TRUE;
    }

  return FALSE;
}

static void
backward_search_finished (GtkSourceSearchContext *search_context,
                          GAsyncResult           *result,
                          gpointer                user_data)
{
  g_autoptr(IdeWordCompletionProvider) self = user_data;
  IdeWordCompletionProviderPrivate *priv = ide_word_completion_provider_get_instance_private (self);
  IdeWordCompletionItem *proposal;
  GtkTextBuffer *buffer = NULL;
  GtkTextIter insert_iter;
  GtkTextIter match_start;
  GtkTextIter match_end;
  GError *error = NULL;
  gboolean has_wrapped_around;

  g_assert (IDE_IS_WORD_COMPLETION_PROVIDER (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (priv->context == NULL || !gtk_source_completion_context_get_iter (priv->context, &insert_iter))
    return;

  buffer = gtk_text_iter_get_buffer (&insert_iter);

  if (gtk_source_search_context_backward_finish2 (search_context,
                                                  result,
                                                  &match_start,
                                                  &match_end,
                                                  &has_wrapped_around,
                                                  &error))
    {
      gchar *text = NULL;

      priv->start_mark = gtk_text_buffer_create_mark (buffer, NULL, &match_start, FALSE);
      priv->end_mark = gtk_text_buffer_create_mark (buffer, NULL, &match_end, FALSE);

      if (priv->start_mark == NULL || priv->end_mark == NULL)
        {
          g_warning ("Cannot set start and end marks for word completion matches.");
          return;
        }

      gtk_source_completion_context_get_iter (priv->context, &insert_iter);

      if (gtk_text_iter_equal (&match_end, &insert_iter) && priv->wrap_around_flag)
        goto finish;

      if (error != NULL)
        {
          g_warning ("Unable to get word completion proposals: %s", error->message);
          g_clear_error (&error);
          goto finish;
        }

      if (!refresh_iters (self, &match_start, &match_end))
        {
          g_warning ("Cannot refresh GtkTextIters for word completion matches.");
          return;
        }

      text = gtk_text_iter_get_text (&match_start, &match_end);

      if (!g_hash_table_contains (priv->all_proposals, text))
        {
          gint offset;

          offset = gtk_text_iter_get_offset (&insert_iter) - gtk_text_iter_get_offset (&match_start);

          /*  Scan must have wrapped around giving offset as negative */
          if (offset < 0)
            {
              GtkTextIter end_iter;

              gtk_text_buffer_get_end_iter (buffer, &end_iter);

              offset = gtk_text_iter_get_offset (&end_iter) -
                       gtk_text_iter_get_offset (&match_start) +
                       gtk_text_iter_get_offset (&insert_iter);

              priv->wrap_around_flag = TRUE;
             }

	  g_assert (offset >= 0);

	  proposal = ide_word_completion_item_new (text, offset, NULL);
          ide_completion_results_take_proposal (IDE_COMPLETION_RESULTS (priv->results),
                                                IDE_COMPLETION_ITEM (proposal));

	  g_hash_table_add (priv->all_proposals, g_steal_pointer (&text));
	}

      gtk_text_buffer_get_iter_at_mark (buffer, &match_end, priv->end_mark);
      gtk_source_search_context_forward_async (priv->search_context,
                                               &match_end,
                                               NULL,
                                               (GAsyncReadyCallback) backward_search_finished,
                                               g_object_ref (self));
      gtk_text_buffer_delete_mark (buffer, priv->start_mark);
      gtk_text_buffer_delete_mark (buffer, priv->end_mark);
      return;
    }

finish:
  ide_completion_results_present (IDE_COMPLETION_RESULTS (priv->results),
                                  GTK_SOURCE_COMPLETION_PROVIDER (self), priv->context);

  g_clear_pointer (&priv->all_proposals, g_hash_table_destroy);
}

static void
forward_search_finished (GtkSourceSearchContext *search_context,
                         GAsyncResult           *result,
                         gpointer                user_data)
{
  g_autoptr(IdeWordCompletionProvider) self = user_data;
  IdeWordCompletionProviderPrivate *priv = ide_word_completion_provider_get_instance_private (self);
  IdeWordCompletionItem *proposal;
  GtkTextBuffer *buffer = NULL;
  GtkTextIter insert_iter;
  GtkTextIter match_start;
  GtkTextIter match_end;
  GError *error = NULL;
  gboolean has_wrapped_around;

  g_assert (IDE_IS_WORD_COMPLETION_PROVIDER (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (priv->context == NULL || !gtk_source_completion_context_get_iter (priv->context, &insert_iter))
    return;

  buffer = gtk_text_iter_get_buffer (&insert_iter);

  if (gtk_source_search_context_forward_finish2 (search_context,
                                                 result,
                                                 &match_start,
                                                 &match_end,
                                                 &has_wrapped_around,
                                                 &error))
    {
      gchar *text = NULL;

      priv->start_mark = gtk_text_buffer_create_mark (buffer, NULL, &match_start, FALSE);
      priv->end_mark = gtk_text_buffer_create_mark (buffer, NULL, &match_end, FALSE);

      if (priv->start_mark == NULL || priv->end_mark == NULL)
        {
          g_warning ("Cannot set start and end marks for word completion matches.");
          return;
        }

      gtk_source_completion_context_get_iter (priv->context, &insert_iter);

      if (gtk_text_iter_equal (&match_end, &insert_iter) && priv->wrap_around_flag)
        goto finish;

      if (error != NULL)
        {
          g_warning ("Unable to get word completion proposals: %s", error->message);
          g_clear_error (&error);
          goto finish;
        }

      if (!refresh_iters (self, &match_start, &match_end))
        {
          g_warning ("Cannot refresh GtkTextIters for word completion matches.");
          return;
        }

      text = gtk_text_iter_get_text (&match_start, &match_end);

      if (!g_hash_table_contains (priv->all_proposals, text))
        {
          gint offset;

          offset = gtk_text_iter_get_offset (&match_start) - gtk_text_iter_get_offset (&insert_iter);

          /*  Scan must have wrapped around giving offset as negative */
          if (offset < 0)
            {
              GtkTextIter end_iter;

              gtk_text_buffer_get_end_iter (buffer, &end_iter);

              offset = gtk_text_iter_get_offset (&end_iter) -
                       gtk_text_iter_get_offset (&insert_iter) +
                       gtk_text_iter_get_offset (&match_start);

              priv->wrap_around_flag = TRUE;
             }

	  g_assert (offset >= 0);

	  proposal = ide_word_completion_item_new (text, offset, NULL);
          ide_completion_results_take_proposal (IDE_COMPLETION_RESULTS (priv->results),
                                                IDE_COMPLETION_ITEM (proposal));

	  g_hash_table_add (priv->all_proposals, g_steal_pointer (&text));
	}

      gtk_text_buffer_get_iter_at_mark (buffer, &match_end, priv->end_mark);
      gtk_source_search_context_forward_async (priv->search_context,
                                               &match_end,
                                               NULL,
                                               (GAsyncReadyCallback) forward_search_finished,
                                               g_object_ref (self));
      gtk_text_buffer_delete_mark (buffer, priv->start_mark);
      gtk_text_buffer_delete_mark (buffer, priv->end_mark);
      return;
    }

finish:
  ide_completion_results_present (IDE_COMPLETION_RESULTS (priv->results),
                                  GTK_SOURCE_COMPLETION_PROVIDER (self), priv->context);

  g_clear_pointer (&priv->all_proposals, g_hash_table_destroy);
}

static void
completion_cleanup (IdeWordCompletionProvider *self)
{
  IdeWordCompletionProviderPrivate *priv = ide_word_completion_provider_get_instance_private (self);

  g_clear_pointer (&priv->current_word, g_free);

  if (priv->context != NULL)
    {
      ide_clear_signal_handler (priv->context, &priv->cancel_id);

      g_clear_object (&priv->context);
    }
  g_clear_object (&priv->search_settings);
  g_clear_object (&priv->search_context);
}

static void
completion_cancelled_cb (IdeWordCompletionProvider  *self,
                         GtkSourceCompletionContext *context)
{
  g_assert (IDE_IS_WORD_COMPLETION_PROVIDER (self));

  if (context == NULL)
    return;

  completion_cleanup (self);
 }

static void
ide_word_completion_provider_populate (GtkSourceCompletionProvider *provider,
                                       GtkSourceCompletionContext  *context)
{
  IdeWordCompletionProvider *self = IDE_WORD_COMPLETION_PROVIDER (provider);
  IdeWordCompletionProviderPrivate *priv = ide_word_completion_provider_get_instance_private (self);
  gchar *search_text = NULL;
  GtkTextIter insert_iter;
  GtkSourceBuffer *buffer;

  if (!gtk_source_completion_context_get_iter (context, &insert_iter))
    {
      gtk_source_completion_context_add_proposals (context, provider, NULL, TRUE);
      IDE_EXIT;
    }

  buffer = GTK_SOURCE_BUFFER (gtk_text_iter_get_buffer (&insert_iter));

  g_assert (priv->search_settings == NULL);
  g_assert (priv->search_context == NULL);
  g_assert (buffer != NULL);
  g_assert (priv->cancel_id == 0);

  g_clear_pointer (&priv->current_word, g_free);
  priv->current_word = ide_completion_provider_context_current_word (context);

  if (priv->current_word == NULL || (g_utf8_strlen (priv->current_word, -1) < (glong)priv->minimum_word_size))
  IDE_EXIT;

  if (priv->results != NULL)
    {
      if (ide_completion_results_replay (IDE_COMPLETION_RESULTS (priv->results), priv->current_word))
        {
          ide_completion_results_present (IDE_COMPLETION_RESULTS (priv->results), provider, context);
          IDE_EXIT;
        }

      g_clear_pointer (&priv->results, g_object_unref);
    }

  priv->search_settings = g_object_new (GTK_SOURCE_TYPE_SEARCH_SETTINGS,
                                        "at-word-boundaries", TRUE,
                                        "regex-enabled", TRUE,
                                        "wrap-around", TRUE,
                                        NULL);

  priv->search_context = gtk_source_search_context_new (buffer, priv->search_settings);
  gtk_source_search_context_set_highlight (priv->search_context, FALSE);
  priv->context = g_object_ref (context);

  search_text = g_strconcat (priv->current_word, "[a-zA-Z0-9_]*", NULL);
  gtk_source_search_settings_set_search_text (priv->search_settings, search_text);
  g_free (search_text);

  priv->cancel_id = g_signal_connect_swapped (context, "cancelled", G_CALLBACK (completion_cancelled_cb), self);
  priv->wrap_around_flag = FALSE;
  priv->results = ide_word_completion_results_new (priv->current_word);

  priv->all_proposals = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  if (priv->direction == 1)  /* Ctrl-n : Scan forward*/
    {
      gtk_source_search_context_forward_async (priv->search_context,
                                               &insert_iter,
                                               NULL,
                                               (GAsyncReadyCallback)forward_search_finished,
                                               g_object_ref (self));

    }
  else if (priv->direction == -1) /* Ctrl-p : Scan backward */
    {
      gtk_source_search_context_backward_async (priv->search_context,
                                                &insert_iter,
                                                NULL,
                                                (GAsyncReadyCallback)backward_search_finished,
                                                g_object_ref (self));

    }
}

static gboolean
ide_word_completion_provider_match (GtkSourceCompletionProvider *provider,
                                    GtkSourceCompletionContext  *context)
{
  IdeWordCompletionProvider *self = (IdeWordCompletionProvider *) provider;
  GtkSourceCompletionActivation activation;
  GtkTextIter iter;

  g_assert (IDE_IS_WORD_COMPLETION_PROVIDER (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  if (!gtk_source_completion_context_get_iter (context, &iter))
    return FALSE;

  activation = gtk_source_completion_context_get_activation (context);

  if (activation == GTK_SOURCE_COMPLETION_ACTIVATION_USER_REQUESTED)
    {
      gunichar ch;

      if (gtk_text_iter_starts_line (&iter))
        return FALSE;

      gtk_text_iter_backward_char (&iter);

      ch = gtk_text_iter_get_char (&iter);

      if (g_unichar_isalnum (ch) || ch == '_')
        return TRUE;
    }

  return FALSE;
}

static gboolean
ide_word_completion_provider_get_start_iter (GtkSourceCompletionProvider *provider,
                                             GtkSourceCompletionContext  *context,
                                             GtkSourceCompletionProposal *proposal,
                                             GtkTextIter                 *iter)
{
  gchar *word;
  glong nb_chars;

  if (!gtk_source_completion_context_get_iter (context, iter))
    return FALSE;

  word = ide_completion_provider_context_current_word (context);
  g_return_val_if_fail (word != NULL, FALSE);

  nb_chars = g_utf8_strlen (word, -1);
  gtk_text_iter_backward_chars (iter, nb_chars);

  g_free (word);
  return TRUE;
}

static gchar *
ide_word_completion_provider_get_name (GtkSourceCompletionProvider *provider)
{
  IdeWordCompletionProvider *self = (IdeWordCompletionProvider *) provider;
  IdeWordCompletionProviderPrivate *priv = ide_word_completion_provider_get_instance_private (self);

  return g_strdup (priv->name);
}

static GIcon *
ide_word_completion_provider_get_gicon (GtkSourceCompletionProvider *provider)
{
  IdeWordCompletionProvider *self = (IdeWordCompletionProvider *) provider;
  IdeWordCompletionProviderPrivate *priv = ide_word_completion_provider_get_instance_private (self);

  return priv->icon;
}

static gint
ide_word_completion_provider_get_interactive_delay (GtkSourceCompletionProvider *provider)
{
  IdeWordCompletionProvider *self = (IdeWordCompletionProvider *) provider;
  IdeWordCompletionProviderPrivate *priv = ide_word_completion_provider_get_instance_private (self);

  return priv->interactive_delay;
}

static gint
ide_word_completion_provider_get_priority (GtkSourceCompletionProvider *provider)
{
  IdeWordCompletionProvider *self = (IdeWordCompletionProvider *) provider;
  IdeWordCompletionProviderPrivate *priv = ide_word_completion_provider_get_instance_private (self);

  return priv->priority;
}

static GtkSourceCompletionActivation
ide_word_completion_provider_get_activation (GtkSourceCompletionProvider *provider)
{
  IdeWordCompletionProvider *self = (IdeWordCompletionProvider *) provider;
  IdeWordCompletionProviderPrivate *priv = ide_word_completion_provider_get_instance_private (self);

  return priv->activation;
}


static void ide_word_completion_provider_iface_init (GtkSourceCompletionProviderIface *iface)
{
  iface->get_name = ide_word_completion_provider_get_name;
  iface->get_gicon = ide_word_completion_provider_get_gicon;
  iface->populate = ide_word_completion_provider_populate;
  iface->match = ide_word_completion_provider_match;
  iface->get_start_iter = ide_word_completion_provider_get_start_iter;
  iface->get_interactive_delay = ide_word_completion_provider_get_interactive_delay;
  iface->get_priority = ide_word_completion_provider_get_priority;
  iface->get_activation = ide_word_completion_provider_get_activation;
}

static void
ide_word_completion_provider_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  IdeWordCompletionProvider *self = IDE_WORD_COMPLETION_PROVIDER (object);
  IdeWordCompletionProviderPrivate *priv = ide_word_completion_provider_get_instance_private (self);

  switch (prop_id)
    {
      case PROP_NAME:
        g_free (priv->name);
        priv->name = g_value_dup_string (value);

        if (priv->name == NULL)
          {
            priv->name = g_strdup (_("Builder Word Completion"));
          }
        break;

      case PROP_ICON:
        g_clear_object (&priv->icon);
        priv->icon = g_value_dup_object (value);
        break;

      case PROP_INTERACTIVE_DELAY:
        priv->interactive_delay = g_value_get_int (value);
        break;

      case PROP_PRIORITY:
        priv->priority = g_value_get_int (value);
        break;

      case PROP_ACTIVATION:
        priv->activation = g_value_get_flags (value);
        break;

      case PROP_DIRECTION:
        priv->direction = g_value_get_int (value);
        break;

      case PROP_MINIMUM_WORD_SIZE:
        priv->minimum_word_size = g_value_get_uint (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
ide_word_completion_provider_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  IdeWordCompletionProvider *self = IDE_WORD_COMPLETION_PROVIDER (object);
  IdeWordCompletionProviderPrivate *priv = ide_word_completion_provider_get_instance_private (self);

  switch (prop_id)
    {
      case PROP_NAME:
        g_value_set_string (value, priv->name);
        break;

      case PROP_ICON:
        g_value_set_object (value, priv->icon);
        break;

      case PROP_INTERACTIVE_DELAY:
        g_value_set_int (value, priv->interactive_delay);
        break;

      case PROP_PRIORITY:
        g_value_set_int (value, priv->priority);
	break;

      case PROP_ACTIVATION:
        g_value_set_flags (value, priv->activation);
        break;

      case PROP_DIRECTION:
        g_value_set_int (value, priv->direction);
        break;

      case PROP_MINIMUM_WORD_SIZE:
        g_value_set_uint (value, priv->minimum_word_size);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
ide_word_completion_provider_dispose (GObject *object)
{
  IdeWordCompletionProvider *self = IDE_WORD_COMPLETION_PROVIDER (object);
  IdeWordCompletionProviderPrivate *priv = ide_word_completion_provider_get_instance_private (self);

  completion_cleanup (self);

  g_free (priv->name);
  priv->name = NULL;

  g_clear_object (&priv->icon);
  g_clear_object (&priv->search_context);
  g_clear_object (&priv->results);

  G_OBJECT_CLASS (ide_word_completion_provider_parent_class)->dispose (object);
}

static void
ide_word_completion_provider_class_init (IdeWordCompletionProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ide_word_completion_provider_get_property;
  object_class->set_property = ide_word_completion_provider_set_property;
  object_class->dispose = ide_word_completion_provider_dispose;

  properties[PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The provider name",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  properties[PROP_ICON] =
    g_param_spec_object ("icon",
                         "Icon",
                         "The provider icon",
                         G_TYPE_ICON,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  properties[PROP_INTERACTIVE_DELAY] =
    g_param_spec_int ("interactive-delay",
                      "Interactive Delay",
                      "The delay before initiating interactive completion",
                      -1,
                      G_MAXINT,
                      50,
                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  properties[PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "Provider priority",
                      G_MININT,
                      G_MAXINT,
                      0,
                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  properties[PROP_ACTIVATION] =
    g_param_spec_flags ("activation",
                        "Activation",
                        "The type of activation",
                        GTK_SOURCE_TYPE_COMPLETION_ACTIVATION,
                        GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE |
                        GTK_SOURCE_COMPLETION_ACTIVATION_USER_REQUESTED,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  properties [PROP_DIRECTION] =
    g_param_spec_int ("direction",
                      "Direction",
                      "The direction for search to begin",
                      G_MININT,
                      G_MAXINT,
                      0,
                      (G_PARAM_READWRITE | G_PARAM_CONSTRUCT |  G_PARAM_STATIC_STRINGS));

  properties[PROP_MINIMUM_WORD_SIZE] =
    g_param_spec_uint ("minimum-word-size",
                       "Minimum Word Size",
                       "The minimum word size to complete",
                       2,
                       G_MAXUINT,
                       2,
                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
ide_word_completion_provider_init (IdeWordCompletionProvider *self)
{
}

IdeWordCompletionProvider *
ide_word_completion_provider_new (const gchar *name,
                                  GIcon       *icon)
{
  return g_object_new (IDE_TYPE_WORD_COMPLETION_PROVIDER,
                       "name", name,
                       "icon", icon,
                       NULL);
}
