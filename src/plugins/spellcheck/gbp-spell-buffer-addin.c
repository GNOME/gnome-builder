/* gbp-spell-buffer-addin.c
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

#define G_LOG_DOMAIN "gbp-spell-buffer-addin"

#include "config.h"

#include <string.h>

#include <libide-code.h>

#include "gbp-spell-buffer-addin.h"

#include "editor-spell-checker.h"
#include "editor-text-buffer-spell-adapter.h"

#define METADATA_SPELLING "metadata::gte-spelling"

struct _GbpSpellBufferAddin
{
  GObject parent_instance;
  IdeBuffer *buffer;
  EditorSpellChecker *checker;
  EditorTextBufferSpellAdapter *adapter;
  GPropertyAction *enabled_action;
  GPropertyAction *language_action;
  guint commit_funcs_handler;
  guint enabled : 1;
};

enum {
  PROP_0,
  PROP_ENABLED,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];
static GSettings *settings;

static gboolean
gbp_spell_buffer_addin_calculate_enabled (GbpSpellBufferAddin *self)
{
  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));

  return self->enabled &&
         self->adapter != NULL &&
         self->buffer != NULL &&
         ide_buffer_get_state (self->buffer) == IDE_BUFFER_STATE_READY;
}

static void
check_error (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GError) error = NULL;

  if (!g_file_set_attributes_finish (file, result, NULL, &error))
    g_warning ("Failed to persist metadata: %s", error->message);
}

static gboolean
state_to_enabled (GBinding     *binding,
                  const GValue *from_value,
                  GValue       *to_value,
                  gpointer      user_data)
{
  GbpSpellBufferAddin *self = user_data;

  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));

  g_value_set_boolean (to_value, gbp_spell_buffer_addin_calculate_enabled (self));

  return TRUE;
}

static void
checker_notify_language_cb (GbpSpellBufferAddin *self,
                            GParamSpec          *pspec,
                            EditorSpellChecker  *spell_checker)
{
  g_autoptr(GFileInfo) info = NULL;
  const char *language_id;
  GFile *file;

  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));
  g_assert (EDITOR_IS_SPELL_CHECKER (spell_checker));
  g_assert (IDE_IS_BUFFER (self->buffer));

  /* Only persist the metadata if we have a backing file */
  if (!(file = ide_buffer_get_file (self->buffer)) || !g_file_is_native (file))
    return;

  /* Ignore if there is nothing to set */
  if (!(language_id = editor_spell_checker_get_language (spell_checker)))
    return;

  info = g_file_info_new ();
  g_file_info_set_attribute_string (info, METADATA_SPELLING, language_id);
  g_file_set_attributes_async (file, info, G_FILE_QUERY_INFO_NONE,
                               G_PRIORITY_DEFAULT, NULL, check_error, NULL);
}

static void
spellcheck_before_insert_text (IdeBuffer *buffer,
                               guint      offset,
                               guint      length,
                               gpointer   user_data)
{
  GbpSpellBufferAddin *self = user_data;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));

  if (self->adapter)
    editor_text_buffer_spell_adapter_before_insert_text (self->adapter, offset, length);
}

static void
spellcheck_after_insert_text (IdeBuffer *buffer,
                              guint      offset,
                              guint      length,
                              gpointer   user_data)
{
  GbpSpellBufferAddin *self = user_data;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));

  if (self->adapter)
    editor_text_buffer_spell_adapter_after_insert_text (self->adapter, offset, length);
}

static void
spellcheck_before_delete_range (IdeBuffer *buffer,
                                guint      offset,
                                guint      length,
                                gpointer   user_data)
{
  GbpSpellBufferAddin *self = user_data;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));

  if (self->adapter)
    editor_text_buffer_spell_adapter_before_delete_range (self->adapter, offset, length);
}

static void
spellcheck_after_delete_range (IdeBuffer *buffer,
                               guint      offset,
                               guint      length,
                               gpointer   user_data)
{
  GbpSpellBufferAddin *self = user_data;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));

  if (self->adapter)
    editor_text_buffer_spell_adapter_after_delete_range (self->adapter, offset, length);
}

static void
gbp_spell_buffer_addin_load (IdeBufferAddin *addin,
                             IdeBuffer      *buffer)
{
  GbpSpellBufferAddin *self = (GbpSpellBufferAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  self->buffer = buffer;
  self->checker = editor_spell_checker_new (NULL, NULL);
  self->adapter = editor_text_buffer_spell_adapter_new (GTK_TEXT_BUFFER (buffer), self->checker);
  self->commit_funcs_handler =
    ide_buffer_add_commit_funcs (buffer,
                                 spellcheck_before_insert_text,
                                 spellcheck_after_insert_text,
                                 spellcheck_before_delete_range,
                                 spellcheck_after_delete_range,
                                 self, NULL);

  g_signal_connect_object (self->checker,
                           "notify::language",
                           G_CALLBACK (checker_notify_language_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->enabled_action = g_property_action_new ("enabled", self, "enabled");
  g_object_bind_property_full (buffer, "state", self->adapter, "enabled",
                               G_BINDING_SYNC_CREATE,
                               state_to_enabled, NULL, self, NULL);

  self->language_action = g_property_action_new ("language", self->checker, "language");

  IDE_EXIT;
}

static void
gbp_spell_buffer_addin_unload (IdeBufferAddin *addin,
                               IdeBuffer      *buffer)
{
  GbpSpellBufferAddin *self = (GbpSpellBufferAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));
  g_assert (IDE_IS_BUFFER (buffer));

  ide_buffer_remove_commit_funcs (buffer, self->commit_funcs_handler);
  self->commit_funcs_handler = 0;

  g_signal_handlers_disconnect_by_func (self->checker,
                                        G_CALLBACK (checker_notify_language_cb),
                                        self);

  g_clear_object (&self->checker);
  g_clear_object (&self->adapter);
  g_clear_object (&self->enabled_action);
  g_clear_object (&self->language_action);

  self->buffer = NULL;

  self->enabled = FALSE;

  IDE_EXIT;
}

static void
buffer_addin_iface_init (IdeBufferAddinInterface *iface)
{
  iface->load = gbp_spell_buffer_addin_load;
  iface->unload = gbp_spell_buffer_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSpellBufferAddin, gbp_spell_buffer_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_BUFFER_ADDIN, buffer_addin_iface_init))

static void
gbp_spell_buffer_addin_set_enabled (GbpSpellBufferAddin *self,
                                    gboolean             enabled)
{
  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));

  enabled = !!enabled;

  if (enabled != self->enabled)
    {
      self->enabled = enabled;
      if (self->adapter != NULL)
        editor_text_buffer_spell_adapter_set_enabled (self->adapter,
                                                      gbp_spell_buffer_addin_calculate_enabled (self));
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENABLED]);
    }
}

static gboolean
gbp_spell_buffer_addin_get_enabled (GbpSpellBufferAddin *self)
{
  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));

  return self->enabled;
}

static void
gbp_spell_buffer_addin_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GbpSpellBufferAddin *self = GBP_SPELL_BUFFER_ADDIN (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, gbp_spell_buffer_addin_get_enabled (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_spell_buffer_addin_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GbpSpellBufferAddin *self = GBP_SPELL_BUFFER_ADDIN (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      gbp_spell_buffer_addin_set_enabled (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_spell_buffer_addin_class_init (GbpSpellBufferAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gbp_spell_buffer_addin_get_property;
  object_class->set_property = gbp_spell_buffer_addin_set_property;

  properties [PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          "Enabled",
                          "If spellcheck is enabled for the buffer",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_spell_buffer_addin_init (GbpSpellBufferAddin *self)
{
  if (settings == NULL)
    {
      settings = g_settings_new ("org.gnome.builder.spelling");
      g_debug ("Spellcheck settings loaded with initial value of %s",
               g_settings_get_boolean (settings, "check-spelling") ? "true" : "false");
    }

  g_settings_bind (settings, "check-spelling",
                   self, "enabled",
                   G_SETTINGS_BIND_GET);
}

void
gbp_spell_buffer_addin_add_word (GbpSpellBufferAddin *self,
                                 const char          *word)
{
  IDE_ENTRY;

  g_return_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self));

  if (word == NULL || word[0] == 0)
    IDE_EXIT;

  editor_spell_checker_add_word (self->checker, word);
  editor_text_buffer_spell_adapter_invalidate_all (self->adapter);

  IDE_EXIT;
}

void
gbp_spell_buffer_addin_ignore_word (GbpSpellBufferAddin *self,
                                    const char          *word)
{
  IDE_ENTRY;

  g_return_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self));

  if (word == NULL || word[0] == 0)
    IDE_EXIT;

  editor_spell_checker_ignore_word (self->checker, word);
  editor_text_buffer_spell_adapter_invalidate_all (self->adapter);

  IDE_EXIT;
}

gboolean
gbp_spell_buffer_addin_check_spelling (GbpSpellBufferAddin *self,
                                       const char          *word)
{
  g_return_val_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self), FALSE);

  if (self->checker != NULL && word != NULL)
    return editor_spell_checker_check_word (self->checker, word, -1);

  return TRUE;
}

char **
gbp_spell_buffer_addin_list_corrections (GbpSpellBufferAddin *self,
                                         const char          *word)
{
  g_return_val_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self), NULL);

  if (self->checker != NULL && word != NULL)
    return editor_spell_checker_list_corrections (self->checker, word);

  return NULL;
}

GAction *
gbp_spell_buffer_addin_get_enabled_action (GbpSpellBufferAddin *self)
{
  g_return_val_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self), NULL);

  return G_ACTION (self->enabled_action);
}

GAction *
gbp_spell_buffer_addin_get_language_action (GbpSpellBufferAddin *self)
{
  g_return_val_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self), NULL);

  return G_ACTION (self->language_action);
}
