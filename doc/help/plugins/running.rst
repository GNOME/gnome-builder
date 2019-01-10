#########################
Extending the Run Manager
#########################

Running projects is managed by the ``Ide.RunManager``.
You can provide special "run handlers" to alter how the applications are executed.

For example, the Sysprof-based profiler uses a custom run handler to ensure that the profiler can monitor the process.
Additionally, Builder's GDB debugger backend will alter the arguments so that GDB is used to launch the program.

Generally, hooking into the run manager involves adding a menu entry to the "Run Button" and a callback to process the launch request.

Let's take a look at a practical example using the Valgrind plugin.

.. code-block:: xml
   :caption: Embed file as a resource matching /org/gnome/Builder/plugins/$module_name/gtk/menus.ui

   <?xml version="1.0" encoding="UTF-8"?>
   <interface>
     <menu id="run-menu">
       <section id="run-menu-section">
         <item>
           <attribute name="id">valgrind-run-handler</attribute>
           <attribute name="after">default-run-handler</attribute>
           <attribute name="action">run-manager.run-with-handler</attribute>
           <attribute name="target">valgrind</attribute>
           <attribute name="label" translatable="yes">Run with Valgrind</attribute>
           <attribute name="verb-icon-name">system-run-symbolic</attribute>
           <attribute name="accel">&lt;Control&gt;F10</attribute>
         </item>
       </section>
     </menu>
   </interface>

For more information on embedding resources with Python-based plugins,
see :ref:`creating embedded GResources<embedding_resources>`.

Now register a run handler to handle the launch request.

.. code-block:: python3

   from gi.repository import Ide
   from gi.repository import GLib
   from gi.repository import GObject

   _ = Ide.gettext

   class ValgrindWorkbenchAddin(GObject.Object, Ide.WorkbenchAddin):
       workbench = None
       has_handler = False

       def do_load(self, workbench):
           self.workbench = workbench

           build_manager = self.workbench.get_context().get_build_manager()
           build_manager.connect('notify::pipeline', self.notify_pipeline)
           self.notify_pipeline(build_manager, None)

       def notify_pipeline(self, build_manager, pspec):
           run_manager = self.workbench.get_context().get_run_manager()

           # When the pipeline changes, we need to check to see if we can find
           # valgrind inside the runtime environment.
           pipeline = build_manager.get_pipeline()
           if pipeline:
               runtime = pipeline.get_config().get_runtime()
               if runtime and runtime.contains_program_in_path('valgrind'):
                   if not self.has_handler:
                       run_manager.add_handler('valgrind', _('Run with Valgrind'), 'system-run-symbolic', '<primary>F10', self.valgrind_handler)
                       self.has_handler = True
                   return

           if self.has_handler:
               run_manager.remove_handler('valgrind')

       def do_unload(self, workbench):
           if self.has_handler:
               run_manager = self.workbench.get_context().get_run_manager()
               run_manager.remove_handler('valgrind')
           self.workbench = None

       def valgrind_handler(self, run_manager, runner):
           # We want to run with valgrind --log-fd=N so that we get the valgrind
           # output redirected to our temp file. Then when the process exits, we
           # we will open the temp file in the builder editor.
           source_fd, name = GLib.file_open_tmp('gnome-builder-valgrind-XXXXXX.txt')
           map_fd = runner.take_fd(source_fd, -1)
           runner.prepend_argv('--track-origins=yes')
           runner.prepend_argv('--log-fd='+str(map_fd))
           runner.prepend_argv('valgrind')
           runner.connect('exited', self.runner_exited, name)

       def runner_exited(self, runner, name):
           # If we weren't unloaded in the meantime, we can open the file using
           # the "editor" hint to ensure the editor opens the file.
           if self.workbench:
               uri = Ide.Uri.new('file://'+name, 0)
               self.workbench.open_uri_async(uri, 'editor', 0, None, None, None)
