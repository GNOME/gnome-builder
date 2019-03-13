###########################
Implementing a Build System
###########################

Builder has support for many build systems such as autotools, meson, cmake, etc. The build system knows how to find build targets (binaries or scripts that are installed) for the runner, knows how to find build flags used by the clang service, and it can define where the build directory is. It also has an associated ``Ide.PipelineAddin`` (see the next section) that specifies how to do operations like build, rebuild, clean, etc.

.. code-block:: python3
   :caption: An outline for a Buildsystem
   :name: basic-buildsystem-py

   import gi

   from gi.repository import Gio, Ide

   class BasicBuildSystem(Ide.Object, Ide.BuildSystem, Gio.AsyncInitable):

       def do_init_async(self, priority, cancel, callback, data=None):
           task = Gio.Task.new(self, cancel, callback)
           task.set_priority(priority)
           # do something, like check if a build file exists
           task.return_boolean(True)

       def do_init_finish(self, result):
           return result.propagate_boolean()

       def do_get_priority(self):
           return 0 # Choose a priority based on other build systems' priority

       def do_get_build_flags_async(self, ifile, cancellable, callback, data=None):
           task = Gio.Task.new(self, cancellable, callback)
           task.ifile = ifile
           task.build_flags = []
           # get the build flags
           task.return_boolean(True)

       def do_get_build_flags_finish(self, result):
           if result.propagate_boolean():
               return result.build_flags


How does Builder know which build system to use for a project? Each has an associated "project file" (configure.ac for autotools) that has to exist in the source directory for the build system to be used. If a project has multiple project files, the priorities of each are used to decide which to use. You can see where the priority is defined in the code above. The project file is defined in the ``.plugin`` file with these lines (in the case of the make plugin):

.. code-block:: none
    :caption: A snippet from a .plugin file
    :name: project-file-filter-snippet

    X-Project-File-Filter-Pattern=Makefile
    X-Project-File-Filter-Name=Makefile Project

When a project has the right file, the build system will be initialized by ``IdeContext`` during its own initialization process.
