############################
Extending the Build Pipeline
############################

Builder uses the concept of a "Build Pipeline" to build a project. The build
pipeline consistes of multiple "phases" and build "stages" run in a given phase.

For example, in the ``Ide.PipelinePhase.DOWNLOADS`` phase, you might have a stage
that downloads and installs the dependencies for your project. The Flatpak
extension does this when building Flatpak-based project configurations.

The various phases of the build pipeline are the following and are always
executed in exactly this sequence up to the requested phase.

Build Phases
============

  - ``Ide.PipelinePhase.PREPARE`` is the first phase of the build pipeline. Use this to create necessary directories and other preparation steps.
  - ``Ide.PipelinePhase.DOWNLOADS`` should be used to download and cache any build artifacts that are needed during the build.
  - ``Ide.PipelinePhase.DEPENDENCIES`` should build any dependencies that are needed to successfully build the project.
  - ``Ide.PipelinePhase.AUTOGEN`` should generate any necessary project files. Contrast this with the ``Ide.PipelinePhase.CONFIGURE`` phase which runs the configuration scripts.
  - ``Ide.PipelinePhase.CONFIGURE`` should run configuration scripts such as ``./configure``, ``meson``, or ``cmake``.
  - ``Ide.PipelinePhase.BUILD`` should perform the incremental build process such as ``make`` or ``ninja``.
  - ``Ide.PipelinePhase.INSTALL`` should install the project to the configured prefix.
  - ``Ide.PipelinePhase.EXPORT`` should be used to attach export hooks such as building a Flatpak bundle, Debian, or RPM package.

Additionally, there are phases which have special meaning.

  - ``Ide.PipelinePhase.BEFORE`` can be XOR'd with any previous phase to indicate it should run as part of the phase, but before the phase has started.
  - ``Ide.PipelinePhase.AFTER`` can be XOR'd with any previous phase to indicate it should run as part of the phase, but after the phase has completed.
  - ``Ide.PipelinePhase.FINISHED`` indicates that a previous build request has finished.
  - ``Ide.PipelinePhase.FAILED`` indicates that a previous build request has failed.

Creating a Build Stage
======================

To add a build stage, we start by creating a build pipeline addin. When it
loads we will register our stage in the appropriate phase.

.. code-block:: python3

   # my_plugin.py

   import gi

   from gi.repository import GObject
   from gi.repository import Ide

   class MyPipelineStage(Ide.Object, Ide.PipelineStage):

       def do_execute(self, pipeline, cancellable):
           """
           This is a synchronous build stage, which will block the
           main loop. If what you need to do is long running, you
           might consider using do_execute_async() and
           do_execute_finish().
           """
           print("Running my build stage!")

       def do_clean_async(self, pipeline, cancellable, callback, data):
           """
           When the user requests that the build pipeline run the
           clean operation (often before a "rebuild"), this function
           will be executed. Use it to delete stale directories, etc.
           """
           task = Gio.Task.new(self, cancellable, callback)
           task.return_boolean(True)

       def do_clean_finish(self, task):
           return task.propagate_boolean()

       def do_query(self, pipeline, cancellable):
           """
           If you need to check if this stage still needs to
           be run, use the query signal to check an external
           resource.

           By default, stages are marked completed after they
           run. That means a second attempt to run the stage
           will be skipped unless set_completed() is set to False.

           If you need to do something asynchronous, call
           self.pause() to pause the stage until the async
           operation has completed, and then call unpause()
           to resume execution of the stage.
           """
           # This will run on every request to run the phase
           self.set_completed(False)

       def do_chain(self, next):
           """
           Sometimes, you have build stages that are next to
           each other in the pipeline and they can be coalesced
           into a single operation.

           One such example is "make" followed by "make install".

           You can detect that here and reduce how much work is
           done by the build pipeline.
           """
           return False

   class MyPipelineAddin(GObject.Object, Ide.PipelineAddin):

       def do_load(self, pipeline):
           stage = MyPipelineStage()
           phase = Ide.PipelinePhase.BUILD | Ide.PipelinePhase.AFTER
           stage_id = pipeline.attach(phase, 100, stage)

           # track() can be used to auto-unregister the phase when
           # the pipeline is removed.
           self.track(stage_id)

