/* gbp-spell-dict.c
 *
 * Copyright 2016 Sébastien Lafargue <slafargue@gnome.org>
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

/* This GObject exists until Gspell handles managing the content of a dictionary */

#include "gbp-spell-dict.h"
#include <enchant.h>
#include <gspell/gspell.h>

#include <libide-editor.h>

typedef enum {
  INIT_NONE,
  INIT_PROCESSING,
  INIT_DONE
} InitStatus;

struct _GbpSpellDict
{
  GObject               parent_instance;

  GspellChecker        *checker;
  EnchantBroker        *broker;
  EnchantDict          *dict;
  const GspellLanguage *language;

  GHashTable           *words;

  InitStatus            init_status;
  guint                 update_needed : 1;
};

G_DEFINE_TYPE (GbpSpellDict, gbp_spell_dict, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CHECKER,
  N_PROPS
};

enum {
  LOADED,
  LAST_SIGNAL
};

static GParamSpec *properties [N_PROPS];
static guint signals [LAST_SIGNAL];

static void read_line_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data);

typedef struct
{
  GbpSpellDict     *self;
  GFile            *file;
  GDataInputStream *data_stream;
  GHashTable       *hash_table;
} TaskState;

static void
task_state_free (gpointer data)
{
  TaskState *state = (TaskState *)data;

  g_assert (state != NULL);

  g_clear_object (&state->file);
  g_clear_pointer (&state->hash_table, g_hash_table_unref);
  g_slice_free (TaskState, state);
}

static void
read_line_async (IdeTask *task)
{
  TaskState *state;

  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);
  g_assert (state != NULL);
  g_assert (state->hash_table != NULL);
  g_assert (GBP_IS_SPELL_DICT (state->self));
  g_assert (G_IS_DATA_INPUT_STREAM (state->data_stream));

  g_data_input_stream_read_line_async (state->data_stream,
                                       ide_task_get_priority (task),
                                       ide_task_get_cancellable (task),
                                       read_line_cb,
                                       task);
}

static void
read_line_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  g_autoptr (IdeTask) task = (IdeTask *)user_data;
  g_autoptr(GError) error = NULL;
  TaskState *state;
  gchar *word;
  gsize len;

  if (ide_task_return_error_if_cancelled (task))
    return;

  state = ide_task_get_task_data (task);
  if (NULL == (word = g_data_input_stream_read_line_finish_utf8 (state->data_stream,
                                                                 result,
                                                                 &len,
                                                                 &error)))
    {
      if (error != NULL)
        {
          /* TODO: check invalid utf8 string to skip it */
          ide_task_return_error (task, g_steal_pointer (&error));
        }
      else
        ide_task_return_pointer (task, g_steal_pointer (&state->hash_table), g_hash_table_unref);
    }
  else
    {
      if (len > 0)
        g_hash_table_add (state->hash_table, word);

      read_line_async (g_steal_pointer (&task));
    }
}

static void
open_file_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  g_autoptr (IdeTask) task = (IdeTask *)user_data;
  g_autoptr(GError) error = NULL;
  GFileInputStream *stream;
  TaskState *state;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (ide_task_return_error_if_cancelled (task))
    return;

  state = ide_task_get_task_data (task);
  if (NULL == (stream = g_file_read_finish (state->file, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  state->data_stream = g_data_input_stream_new (G_INPUT_STREAM (stream));
  read_line_async (g_steal_pointer (&task));
}

gboolean
gbp_spell_dict_personal_contains (GbpSpellDict *self,
                                  const gchar  *word)
{
  g_assert (GBP_IS_SPELL_DICT (self));

  if (self->words != NULL && !dzl_str_empty0 (word))
    return g_hash_table_contains (self->words, word);

  return FALSE;
}

gboolean
gbp_spell_dict_add_word_to_personal (GbpSpellDict *self,
                                     const gchar  *word)
{
  g_assert (GBP_IS_SPELL_DICT (self));
  g_assert (!dzl_str_empty0 (word));

 if (self->dict != NULL)
    {
      if (gbp_spell_dict_personal_contains (self, word))
        return FALSE;

      enchant_dict_add (self->dict, word, -1);
      g_hash_table_add (self->words, g_strdup (word));
      return TRUE;
    }
  else
    {
      g_warning ("No dictionaries loaded, cannot add word");
      return FALSE;
    }
}

gboolean
gbp_spell_dict_remove_word_from_personal (GbpSpellDict *self,
                                          const gchar  *word)
{
  g_assert (GBP_IS_SPELL_DICT (self));
  g_assert (!dzl_str_empty0 (word));

  if (self->dict != NULL)
    {
      if (!gbp_spell_dict_personal_contains (self, word) || self->words == NULL)
        return FALSE;

      enchant_dict_remove (self->dict, word, -1);
      g_hash_table_remove (self->words, word);
      return TRUE;
    }
  else
    {
      g_warning ("No dictionaries loaded");
      return FALSE;
    }
}

static void
hash_table_foreach_cb (const gchar *key,
                       const gchar *value,
                       GPtrArray   *ar)
{
  g_ptr_array_add (ar, g_strdup (key));
}

GPtrArray *
gbp_spell_dict_get_words (GbpSpellDict  *self)
{
  GPtrArray *array;

  g_assert (GBP_IS_SPELL_DICT (self));

  if (self->init_status == INIT_NONE)
    {
      g_warning ("Dict not loaded yet, you need to connect and wait for GbpSpellDict::loaded");
      return NULL;
    }

  if (self->words == NULL)
    return NULL;

  array = g_ptr_array_new_with_free_func (g_free);
  g_hash_table_foreach (self->words, (GHFunc)hash_table_foreach_cb, array);

  return array;
}

static void
gbp_spell_dict_get_words_async (GbpSpellDict        *self,
                                GAsyncReadyCallback  callback,
                                GCancellable        *cancellable,
                                gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *path = NULL;
  g_autofree gchar *dict_filename = NULL;
  TaskState *state;
  gint priority;

  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (GBP_IS_SPELL_DICT (self));
  g_assert (callback != NULL);

  state = g_slice_new0 (TaskState);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_spell_dict_get_words_async);
  ide_task_set_task_data (task, state, task_state_free);

  dict_filename = g_strconcat (gspell_language_get_code (self->language), ".dic", NULL);
  path = g_build_filename (g_get_user_config_dir (), "enchant", dict_filename, NULL);
  state->self = self;
  state->hash_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  state->file = g_file_new_for_path (path);

  priority = ide_task_get_priority (task);
  g_file_read_async (state->file,
                     priority,
                     cancellable,
                     open_file_cb,
                     g_steal_pointer (&task));
}

static GHashTable *
gbp_spell_dict_get_words_finish (GbpSpellDict  *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_assert (GBP_IS_SPELL_DICT (self));
  g_assert (ide_task_is_valid (result, self));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
gbp_spell_dict_get_dict_words_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GbpSpellDict  *self = (GbpSpellDict *)user_data;
  g_autoptr(GError) error = NULL;
  GHashTable *words;

  g_assert (GBP_IS_SPELL_DICT (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (NULL == (words = gbp_spell_dict_get_words_finish (self, result, &error)))
    {
      g_debug ("error: %s\n", error->message);
      self->init_status = INIT_DONE;
    }
  else
    {
      g_clear_pointer (&self->words, g_hash_table_unref);
      self->words = g_hash_table_ref (words);
      self->init_status = INIT_DONE;

      g_signal_emit (self, signals [LOADED], 0);
    }

  if (self->update_needed == TRUE)
    {
      self->update_needed = FALSE;
      self->init_status = INIT_PROCESSING;
      gbp_spell_dict_get_words_async (self,
                                      gbp_spell_dict_get_dict_words_cb,
                                      NULL,
                                      self);
    }
}

static void
gbp_spell_dict_set_dict (GbpSpellDict         *self,
                         const GspellLanguage *language)
{
  const gchar *lang_code;

  g_assert (GBP_IS_SPELL_DICT (self));

  if (language != NULL)
    {
      lang_code = gspell_language_get_code (language);
      self->dict = enchant_broker_request_dict (self->broker, lang_code);

      if (self->init_status == INIT_PROCESSING)
        self->update_needed = TRUE;
      else
        {
          self->init_status = INIT_PROCESSING;
          gbp_spell_dict_get_words_async (self,
                                                 gbp_spell_dict_get_dict_words_cb,
                                                 NULL,
                                                 self);
        }
    }
  else if (self->dict != NULL)
    {
      enchant_broker_free_dict (self->broker, self->dict);
      self->dict = NULL;

      g_clear_pointer (&self->words, g_hash_table_unref);
    }
}

static void
language_notify_cb (GbpSpellDict  *self,
                    GParamSpec    *pspec,
                    GspellChecker *checker)
{
  const GspellLanguage *language;

  g_assert (GBP_IS_SPELL_DICT (self));
  g_assert (GSPELL_IS_CHECKER (checker));

  language = gspell_checker_get_language (self->checker);

  if ((self->language == NULL && language != NULL) ||
      (self->language != NULL && language == NULL) ||
      0 != gspell_language_compare (language, self->language))
    {
      self->language = language;
      gbp_spell_dict_set_dict (self, language);
    }
}

static void
checker_weak_ref_cb (gpointer  data,
                     GObject  *where_the_object_was)
{
  GbpSpellDict *self = (GbpSpellDict *)data;

  g_assert (GBP_IS_SPELL_DICT (self));

  self->checker = NULL;
  self->language = NULL;
  gbp_spell_dict_set_dict (self, NULL);
}

GspellChecker *
gbp_spell_dict_get_checker (GbpSpellDict *self)
{
  g_return_val_if_fail (GBP_IS_SPELL_DICT (self), NULL);

  return self->checker;
}

void
gbp_spell_dict_set_checker (GbpSpellDict  *self,
                            GspellChecker *checker)
{
  g_return_if_fail (GBP_IS_SPELL_DICT (self));

  if (self->checker != checker)
    {
      if (self->checker != NULL)
        g_object_weak_unref (G_OBJECT (self->checker), checker_weak_ref_cb, self);

      if (checker == NULL)
        {
          checker_weak_ref_cb (self, NULL);
          return;
        }

      self->checker = checker;
      g_object_weak_ref (G_OBJECT (self->checker), checker_weak_ref_cb, self);
      g_signal_connect_object (self->checker,
                               "notify::language",
                               G_CALLBACK (language_notify_cb),
                               self,
                               G_CONNECT_SWAPPED);

      language_notify_cb (self, NULL, self->checker);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CHECKER]);
    }
}

GbpSpellDict *
gbp_spell_dict_new (GspellChecker *checker)
{
  return g_object_new (GBP_TYPE_SPELL_DICT,
                       "checker", checker,
                       NULL);
}

static void
gbp_spell_dict_finalize (GObject *object)
{
  GbpSpellDict *self = (GbpSpellDict *)object;

  if (self->broker != NULL)
    {
      if (self->dict != NULL)
        enchant_broker_free_dict (self->broker, self->dict);
      g_clear_pointer (&self->broker, enchant_broker_free);
    }

  if (self->words != NULL)
    {
      g_hash_table_remove_all (self->words);
      g_clear_pointer (&self->words, g_hash_table_unref);
    }

  G_OBJECT_CLASS (gbp_spell_dict_parent_class)->finalize (object);
}

static void
gbp_spell_dict_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbpSpellDict *self = GBP_SPELL_DICT (object);

  switch (prop_id)
    {
    case PROP_CHECKER:
      g_value_set_object (value, gbp_spell_dict_get_checker (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_spell_dict_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbpSpellDict *self = GBP_SPELL_DICT (object);

  switch (prop_id)
    {
    case PROP_CHECKER:
      gbp_spell_dict_set_checker (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_spell_dict_loaded (GbpSpellDict *self)
{
  g_assert (GBP_IS_SPELL_DICT (self));
}

static void
gbp_spell_dict_class_init (GbpSpellDictClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_spell_dict_finalize;
  object_class->get_property = gbp_spell_dict_get_property;
  object_class->set_property = gbp_spell_dict_set_property;

  properties [PROP_CHECKER] =
   g_param_spec_object ("checker",
                        "Checker",
                        "Checker",
                        GSPELL_TYPE_CHECKER,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * GbpSpellDict::loaded:
   *
   * This signal is emitted when the dictionary is fully initialised.
   * (for now, personal dictionary loaded)
   */
  signals [LOADED] =
    g_signal_new_class_handler ("loaded",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (gbp_spell_dict_loaded),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                0);
}

static void
gbp_spell_dict_init (GbpSpellDict *self)
{
  self->broker = enchant_broker_init ();
}
