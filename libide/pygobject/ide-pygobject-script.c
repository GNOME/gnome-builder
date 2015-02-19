/* ide-pygobject-script.c
 *
 * Copyright (C) 2015 Garrett Regier <garrettregier@gmail.com>
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

#include "ide-context.h"
#include "ide-pygobject-script.h"

/* _POSIX_C_SOURCE is defined in Python.h and in limits.h included by
 * glib-object.h, so we unset it here to avoid a warning. Yep, that's bad.
 */
#undef _POSIX_C_SOURCE
#include <pygobject.h>

#include <glib/gi18n.h>

struct _IdePyGObjectScript
{
  IdeScript parent_instance;
};

static PyThreadState *py_thread_state = NULL;

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (IdePyGObjectScript, ide_pygobject_script, IDE_TYPE_SCRIPT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                               async_initable_iface_init))

static gboolean
init_pygobject (void)
{
  PyGILState_STATE state = 0;
  long hexversion;
  gboolean must_finalize_python = FALSE;
  static gboolean initialized = FALSE;
  static gboolean success = FALSE;

  if (initialized)
    return success;

  initialized = TRUE;

  /* Python initialization */
  if (Py_IsInitialized ())
    {
      state = PyGILState_Ensure ();
    }
  else
    {
      Py_InitializeEx (FALSE);
      must_finalize_python = TRUE;
    }

  hexversion = PyLong_AsLong (PySys_GetObject ((char *) "hexversion"));

#if PY_VERSION_HEX < 0x03000000
  if (hexversion >= 0x03000000)
#else
  if (hexversion < 0x03000000)
#endif
    {
      g_critical ("Attempting to mix incompatible Python versions");
      return FALSE;
    }

  /* Initialize PyGObject */
  pygobject_init (3, 0, 0);

  if (PyErr_Occurred ())
    {
      g_warning ("PyGObject initialization failed");
      PyErr_Print ();
      return FALSE;
    }

  /* Initialize support for threads */
  pyg_enable_threads ();
  PyEval_InitThreads ();

  /* Only redirect warnings when pygobject was not already initialized */
  if (!must_finalize_python)
    pyg_disable_warning_redirections ();

  if (!must_finalize_python)
    PyGILState_Release (state);
  else
    py_thread_state = PyEval_SaveThread ();

  success = TRUE;
  return TRUE;
}

static void
ide_pygobject_script_load (IdeScript *script)
{
  IdePyGObjectScript *self = (IdePyGObjectScript *)script;
  IdeContext *context;
  g_autoptr(GError) error = NULL;
  g_autoptr(gchar) contents = NULL;
  g_autoptr(gchar) path = NULL;
  g_autoptr(GFile) parent = NULL;
  g_autoptr(gchar) parent_path = NULL;
  GFile *file;
  PyObject *globals = NULL;
  PyObject *builtins_module;
  PyObject *module_dir = NULL;
  PyObject *retval;
  PyObject *pycontext = NULL;
  PyObject *code;
  PyGILState_STATE state;

  g_return_if_fail (IDE_IS_PYGOBJECT_SCRIPT (self));

  file = ide_script_get_file (IDE_SCRIPT (script));

  if (!file)
    {
      g_warning (_("Attempt to load a PyGObject script with no filename."));
      return;
    }

  path = g_file_get_basename (file);

  if (!g_file_load_contents (file, NULL, &contents, NULL, NULL, &error))
    {
      g_warning ("%s", error->message);
      return;
    }

  if (!init_pygobject ())
    return;

  state = PyGILState_Ensure ();

  globals = PyDict_New ();
  if (globals == NULL)
    goto out;

  builtins_module = PyImport_ImportModule ("builtins");
  if (builtins_module == NULL)
    goto out;

  if (PyDict_SetItemString (globals, "__builtins__", builtins_module) != 0)
    goto out;

  parent = g_file_get_parent (file);
  parent_path = g_file_get_path (parent);
  module_dir = PyUnicode_FromString (parent_path);

  if (PyDict_SetItemString (globals, "module_dir", module_dir) != 0)
    goto out;

  retval = PyRun_String ("import signal\n"
                         "import sys\n"
                         "if module_dir not in sys.path:\n"
                         "    sys.path.insert(0, module_dir)\n"
                         "\n"
                         "signal.signal(signal.SIGINT, signal.SIG_DFL)\n",
                         Py_file_input,
                         globals, globals);

  if (PyDict_DelItemString (globals, "module_dir") != 0)
    goto out;

  context = ide_object_get_context (IDE_OBJECT (self));
  pycontext = pygobject_new (G_OBJECT (context));
  if (pycontext == NULL)
    goto out;

  if (PyDict_SetItemString (globals, "Context", pycontext) != 0)
    goto out;

  code = Py_CompileString (contents, path, Py_file_input);
  if (code == NULL)
    goto out;

  retval = PyEval_EvalCode (code, globals, globals);
  Py_XDECREF (retval);

out:

  Py_XDECREF (code);
  Py_XDECREF (pycontext);
  Py_XDECREF (module_dir);
  Py_XDECREF (globals);

  if (PyErr_Occurred ())
    PyErr_Print ();

  PyGILState_Release (state);
}

static void
ide_pygobject_script_unload (IdeScript *self)
{
  g_return_if_fail (IDE_IS_PYGOBJECT_SCRIPT (self));
}

static void
ide_pygobject_script_class_init (IdePyGObjectScriptClass *klass)
{
  IdeScriptClass *script_class = IDE_SCRIPT_CLASS (klass);

  script_class->load = ide_pygobject_script_load;
  script_class->unload = ide_pygobject_script_unload;
}

static void
ide_pygobject_script_init (IdePyGObjectScript *self)
{
}

static void
ide_pygobject_script_init_async (GAsyncInitable      *initable,
                           gint                 io_priority,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  IdePyGObjectScript *self = (IdePyGObjectScript *)initable;
  g_autoptr(GTask) task = NULL;
  g_autoptr(gchar) path = NULL;
  GFile *file;

  g_return_if_fail (IDE_IS_PYGOBJECT_SCRIPT (self));
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

  if (!g_str_has_suffix (path, ".py"))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("The script \"%s\" is not a PyGObject file."),
                               path);
      return;
    }

  ide_script_load (IDE_SCRIPT (self));

  g_task_return_boolean (task, TRUE);
}

static gboolean
ide_pygobject_script_init_finish (GAsyncInitable  *initable,
                            GAsyncResult    *result,
                            GError         **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (IDE_IS_PYGOBJECT_SCRIPT (initable), FALSE);
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return g_task_propagate_boolean (task, error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_pygobject_script_init_async;
  iface->init_finish = ide_pygobject_script_init_finish;
}
