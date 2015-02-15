/* ide-gjs-script.c
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

#define GLIB_DISABLE_DEPRECATION_WARNINGS
#include <gjs/gjs.h>
#include <gjs/gjs-module.h>
#include <gi/object.h>
#include <glib/gi18n.h>
#undef GLIB_DISABLE_DEPRECATION_WARNINGS

#include <jsapi.h>

#include "ide-gjs-script.h"

struct _IdeGjsScript
{
  IdeScript parent_instance;

  GFile      *file;
  GjsContext *gjs;
};

G_DEFINE_TYPE (IdeGjsScript, ide_gjs_script, IDE_TYPE_SCRIPT)

enum {
  PROP_0,
  PROP_FILE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

/**
 * ide_gjs_script_get_file:
 *
 * Retrieves the #GFile containing the script to be loaded in the context.
 *
 * Returns: (transfer none): A #GFile.
 */
GFile *
ide_gjs_script_get_file (IdeGjsScript *self)
{
  g_return_val_if_fail (IDE_IS_GJS_SCRIPT (self), NULL);

  return self->file;
}

static void
ide_gjs_script_set_file (IdeGjsScript *self,
                         GFile        *file)
{
  g_return_if_fail (IDE_IS_GJS_SCRIPT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!self->file);

  self->file = (GFile *)g_object_ref (file);
}

static void
set_global (GjsContext  *context,
            const gchar *name,
            GObject     *value)
{
  JSContext *jscontext;
  JSObject *jsglobal;
  jsval jsvalue;

  jscontext = (JSContext *)gjs_context_get_native_context (context);
  jsglobal = (JSObject *)gjs_get_global_object (jscontext);
  jsvalue.setObject (*gjs_object_from_g_object (jscontext, value));

  if (!JS_SetProperty (jscontext, jsglobal, "Context", &jsvalue))
    {
      g_warning (_("Failed to set IdeContext in JavaScript runtime."));
      return;
    }
}

static void
ide_gjs_script_load (IdeScript *script)
{
  IdeGjsScript *self = (IdeGjsScript *)script;
  IdeContext *context;
  g_autoptr(GError) error = NULL;
  g_autoptr(gchar) contents = NULL;
  g_autoptr(gchar) path;
  gsize len;
  int exit_status = 0;

  g_return_if_fail (IDE_IS_GJS_SCRIPT (self));
  g_return_if_fail (!self->gjs);
  g_return_if_fail (G_IS_FILE (self->file));

  if (!self->file)
    {
      g_warning (_("Attempt to load a GJS script with no filename."));
      return;
    }

  path = g_file_get_basename (self->file);

  if (!g_file_load_contents (self->file, NULL, &contents, &len, NULL, &error))
    {
      g_warning ("%s", error->message);
      return;
    }

  self->gjs = gjs_context_new ();

  if (!self->gjs)
    {
      g_warning (_("Failed to create JavaScript context."));
      return;
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  set_global (self->gjs, "Context", G_OBJECT (context));

  if (!gjs_context_eval (self->gjs, contents, len, path, &exit_status, &error))
    {
      g_warning ("%s", error->message);
      return;
    }

  g_info ("GJS script \"%s\" loaded.", path);
}

static void
ide_gjs_script_unload (IdeScript *self)
{
  g_return_if_fail (IDE_IS_GJS_SCRIPT (self));
}

static void
ide_gjs_script_finalize (GObject *object)
{
  IdeGjsScript *self = (IdeGjsScript *)object;

  g_clear_object (&self->file);

  G_OBJECT_CLASS (ide_gjs_script_parent_class)->finalize (object);
}

static void
ide_gjs_script_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeGjsScript *self = IDE_GJS_SCRIPT (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, ide_gjs_script_get_file (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_gjs_script_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeGjsScript *self = IDE_GJS_SCRIPT (object);

  switch (prop_id)
    {
    case PROP_FILE:
      ide_gjs_script_set_file (self, (GFile *)g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_gjs_script_class_init (IdeGjsScriptClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeScriptClass *script_class = IDE_SCRIPT_CLASS (klass);

  object_class->finalize = ide_gjs_script_finalize;
  object_class->get_property = ide_gjs_script_get_property;
  object_class->set_property = ide_gjs_script_set_property;

  script_class->load = ide_gjs_script_load;
  script_class->unload = ide_gjs_script_unload;

  gParamSpecs [PROP_FILE] =
    g_param_spec_object ("file",
                         _("File"),
                         _("The file containing the script contents."),
                         G_TYPE_FILE,
                         (GParamFlags)(G_PARAM_READWRITE |
                                       G_PARAM_CONSTRUCT_ONLY |
                                       G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FILE,
                                   gParamSpecs [PROP_FILE]);
}

static void
ide_gjs_script_init (IdeGjsScript *self)
{
}
