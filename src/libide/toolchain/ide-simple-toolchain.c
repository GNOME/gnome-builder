/* ide-simple-toolchain.c
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright 2018 Collabora Ltd.
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

#define G_LOG_DOMAIN "ide-simple-toolchain"

#include "config.h"

#include "ide-context.h"

#include "toolchain/ide-simple-toolchain.h"

typedef struct
{
  GHashTable *tools;
} IdeSimpleToolchainPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSimpleToolchain, ide_simple_toolchain, IDE_TYPE_TOOLCHAIN)

IdeSimpleToolchain *
ide_simple_toolchain_new (IdeContext   *context,
                          const gchar  *id)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  return g_object_new (IDE_TYPE_SIMPLE_TOOLCHAIN,
                       "context", context,
                       "id", id,
                       NULL);
}

const gchar *
ide_simple_toolchain_get_tool_for_language (IdeToolchain  *toolchain,
                                            const gchar   *language,
                                            const gchar   *tool_id)
{
  IdeSimpleToolchain *self = (IdeSimpleToolchain *)toolchain;
  IdeSimpleToolchainPrivate *priv;

  g_return_val_if_fail (IDE_IS_SIMPLE_TOOLCHAIN (self), NULL);
  g_return_val_if_fail (tool_id != NULL, NULL);

  priv = ide_simple_toolchain_get_instance_private (self);
  return NULL;
}

/**
 * ide_simple_toolchain_set_tool_for_language:
 * @self: an #IdeSimpleToolchain
 * @tool_id: the identifier of the tool like %IDE_TOOLCHAIN_TOOL_CC
 * @language: the language of the tool like %IDE_TOOLCHAIN_LANGUAGE_C.
 * @tool_path: The path of
 *
 * Gets the path of the compiler executable
 *
 * Since: 3.30
 */
void
ide_simple_toolchain_set_tool_for_language  (IdeSimpleToolchain  *self,
                                             const gchar         *language,
                                             const gchar         *tool_id,
                                             const gchar         *tool_path)
{
  IdeSimpleToolchainPrivate *priv;

  g_return_if_fail (IDE_IS_SIMPLE_TOOLCHAIN (self));
  g_return_if_fail (tool_id != NULL);

  priv = ide_simple_toolchain_get_instance_private (self);
  g_hash_table_insert (priv->tools, g_strconcat (tool_id, ":", language, NULL), g_strdup (tool_path));
}

static void
ide_simple_toolchain_finalize (GObject *object)
{
  IdeSimpleToolchain *self = (IdeSimpleToolchain *)object;
  IdeSimpleToolchainPrivate *priv = ide_simple_toolchain_get_instance_private (self);

  g_clear_pointer (&priv->tools, g_hash_table_unref);

  G_OBJECT_CLASS (ide_simple_toolchain_parent_class)->finalize (object);
}

static void
ide_simple_toolchain_class_init (IdeSimpleToolchainClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeToolchainClass *toolchain_class = IDE_TOOLCHAIN_CLASS (klass);

  object_class->finalize = ide_simple_toolchain_finalize;

  toolchain_class->get_tool_for_language = ide_simple_toolchain_get_tool_for_language;
}

static void
ide_simple_toolchain_init (IdeSimpleToolchain *self)
{
  IdeSimpleToolchainPrivate *priv = ide_simple_toolchain_get_instance_private (self);
  priv->tools = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}
