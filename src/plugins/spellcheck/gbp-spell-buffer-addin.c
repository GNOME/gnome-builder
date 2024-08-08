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

#include <libspelling.h>

#include <libide-code.h>

#include "gbp-spell-buffer-addin.h"

#define METADATA_SPELLING "metadata::gte-spelling"

struct _GbpSpellBufferAddin
{
  GObject                    parent_instance;
  IdeBuffer                 *buffer;
  SpellingTextBufferAdapter *adapter;
  SpellingChecker           *checker;
};

static GSettings *settings;

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

static void
checker_notify_language_cb (GbpSpellBufferAddin *self,
                            GParamSpec          *pspec,
                            SpellingChecker     *spell_checker)
{
  g_autoptr(GFileInfo) info = NULL;
  const char *language_id;
  GFile *file;

  g_assert (GBP_IS_SPELL_BUFFER_ADDIN (self));
  g_assert (SPELLING_IS_CHECKER (spell_checker));
  g_assert (IDE_IS_BUFFER (self->buffer));

  /* Only persist the metadata if we have a backing file */
  if (!(file = ide_buffer_get_file (self->buffer)) || !g_file_is_native (file))
    return;

  /* Ignore if there is nothing to set */
  if (!(language_id = spelling_checker_get_language (spell_checker)))
    return;

  info = g_file_info_new ();
  g_file_info_set_attribute_string (info, METADATA_SPELLING, language_id);
  g_file_set_attributes_async (file, info, G_FILE_QUERY_INFO_NONE,
                               G_PRIORITY_DEFAULT, NULL, check_error, NULL);
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
  self->checker = spelling_checker_new (NULL, NULL);
  self->adapter = spelling_text_buffer_adapter_new (GTK_SOURCE_BUFFER (buffer),
                                                    self->checker);

  g_settings_bind (settings, "check-spelling",
                   self->adapter, "enabled",
                   G_SETTINGS_BIND_GET);

  g_signal_connect_object (self->checker,
                           "notify::language",
                           G_CALLBACK (checker_notify_language_cb),
                           self,
                           G_CONNECT_SWAPPED);

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

  g_signal_handlers_disconnect_by_func (self->checker,
                                        G_CALLBACK (checker_notify_language_cb),
                                        self);

  g_clear_object (&self->checker);
  g_clear_object (&self->adapter);

  self->buffer = NULL;

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
gbp_spell_buffer_addin_class_init (GbpSpellBufferAddinClass *klass)
{
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
}

GMenuModel *
gbp_spell_buffer_addin_get_menu (GbpSpellBufferAddin *self)
{
  g_return_val_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self), NULL);

  if (self->adapter == NULL)
    return NULL;

  return spelling_text_buffer_adapter_get_menu_model (self->adapter);
}

void
gbp_spell_buffer_addin_update_menu (GbpSpellBufferAddin *self)
{
  g_return_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self));

  if (self->adapter == NULL)
    return;

  spelling_text_buffer_adapter_update_corrections (self->adapter);
}

GActionGroup *
gbp_spell_buffer_addin_get_actions (GbpSpellBufferAddin *self)
{
  g_return_val_if_fail (GBP_IS_SPELL_BUFFER_ADDIN (self), NULL);

  return G_ACTION_GROUP (self->adapter);
}
