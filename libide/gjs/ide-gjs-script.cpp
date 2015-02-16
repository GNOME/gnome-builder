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

#include "ide-context.h"
#include "ide-gjs-script.h"

struct _IdeGjsScript
{
  IdeScript   parent_instance;
  GjsContext *gjs;
};

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeGjsScript, ide_gjs_script, IDE_TYPE_SCRIPT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               async_initable_iface_init))

static const char *init_js_code = "imports.gi.Ide;\n";

static void
ide_gjs_script_load (IdeScript *script)
{
  IdeGjsScript *self = (IdeGjsScript *)script;
  IdeContext *context;
  GjsContext *old_current;
  g_autoptr(GError) error = NULL;
  g_autoptr(gchar) contents = NULL;
  g_autoptr(gchar) path;
  g_autoptr(GFile) parent;
  gchar **search_path;
  GFile *file;
  gsize len;
  int exit_status = 0;

  g_return_if_fail (IDE_IS_GJS_SCRIPT (self));
  g_return_if_fail (!self->gjs);

  file = ide_script_get_file (IDE_SCRIPT (script));

  if (!file)
    {
      g_warning (_("Attempt to load a GJS script with no filename."));
      return;
    }

  path = g_file_get_basename (file);

  if (!g_file_load_contents (file, NULL, &contents, &len, NULL, &error))
    {
      g_warning ("%s", error->message);
      return;
    }

  old_current = gjs_context_get_current ();
  if (old_current)
    gjs_context_make_current (NULL);

  parent = g_file_get_parent (file);
  search_path = g_new0 (gchar*, 2);
  search_path[0] = g_file_get_path (parent);
  search_path[1] = NULL;
  self->gjs = (GjsContext *)g_object_new (GJS_TYPE_CONTEXT,
                                          "search-path", search_path,
                                          NULL);
  g_strfreev (search_path);

  if (!self->gjs)
    {
      g_warning (_("Failed to create JavaScript context."));
      return;
    }

  context = ide_object_get_context (IDE_OBJECT (self));

  JSContext *jscontext;
  JSObject *jsglobal;

  jscontext = (JSContext *)gjs_context_get_native_context (self->gjs);
  jsglobal = (JSObject *)gjs_get_global_object (jscontext);

  JSAutoCompartment ac(jscontext, jsglobal);
  JSAutoRequest ar(jscontext);
  jsval jsvalue;

  g_assert (IDE_IS_CONTEXT (context));
  g_assert (jscontext);

  jsvalue.setObject (*gjs_object_from_g_object (jscontext, G_OBJECT (context)));

  gjs_context_eval (self->gjs, init_js_code, strlen(init_js_code), "<init>", NULL, NULL);

  if (!JS_SetProperty (jscontext, jsglobal, "Context", &jsvalue))
    {
      g_warning (_("Failed to set IdeContext in JavaScript runtime."));
      return;
    }

  if (!gjs_context_eval (self->gjs, contents, len, path, &exit_status, &error))
    {
      g_warning ("%s", error->message);
      return;
    }

  if (old_current != NULL)
    {
      gjs_context_make_current (NULL);
      gjs_context_make_current (old_current);
    }
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

  g_clear_object (&self->gjs);

  G_OBJECT_CLASS (ide_gjs_script_parent_class)->finalize (object);
}

static void
ide_gjs_script_class_init (IdeGjsScriptClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeScriptClass *script_class = IDE_SCRIPT_CLASS (klass);

  object_class->finalize = ide_gjs_script_finalize;

  script_class->load = ide_gjs_script_load;
  script_class->unload = ide_gjs_script_unload;
}

static void
ide_gjs_script_init (IdeGjsScript *self)
{
}

static void
ide_gjs_script_init_async (GAsyncInitable      *initable,
                           gint                 io_priority,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  IdeGjsScript *self = (IdeGjsScript *)initable;
  g_autoptr(GTask) task = NULL;
  g_autoptr(gchar) path = NULL;
  GFile *file;

  g_return_if_fail (IDE_IS_GJS_SCRIPT (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  file = ide_script_get_file (IDE_SCRIPT (self));

  if (!file)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               _("The filename for the script was not provided."));
      return;
    }

  path = g_file_get_path (file);

  if (!path)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_FILENAME,
                               _("The script must be on a local filesystem."));
      return;
    }

  ide_script_load (IDE_SCRIPT (self));

  g_task_return_boolean (task, TRUE);
}

static gboolean
ide_gjs_script_init_finish (GAsyncInitable  *initable,
                            GAsyncResult    *result,
                            GError         **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_GJS_SCRIPT (initable), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_gjs_script_init_async;
  iface->init_finish = ide_gjs_script_init_finish;
}
