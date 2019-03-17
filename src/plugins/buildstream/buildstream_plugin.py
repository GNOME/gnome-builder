from gi.repository import Ide
from gi.repository import Gio
from gi.repository import GObject

class BuildStreamBuildSystemDiscovery(Ide.SimpleBuildSystemDiscovery):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.props.glob = 'project.conf'
        self.props.hint = 'buildstream_plugin'
        self.props.priority = 2000

class BuildStreamBuildSystem(Ide.Object, Ide.BuildSystem):
    project_file = GObject.Property(type=Gio.File)

    def do_get_id(self):
        return 'BuildStream'

    def do_get_display(self):
        return 'BuildStream'

    def do_get_priority(self):
        return 2000

class BuildStreamPipelineAddin(Ide.Object, Ide.PipelineAddin):

    def do_load(self, pipeline):

        context = self.get_context()

        build_system = Ide.BuildSystem.from_context(context)

        if type(build_system) != BuildStreamBuildSystem:
            return

        config = pipeline.get_config()
        runtime = config.get_runtime()
        srcdir = pipeline.get_srcdir()

        if not runtime.contains_program_in_path('bst'):
            raise OSError('The runtime must contain bst to build BuildStream projects')

        build_launcher = pipeline.create_launcher()
        build_launcher.set_cwd(srcdir)
        build_launcher.push_argv('bst')
        build_launcher.push_argv('build')

        build_stage = Ide.PipelineStageLauncher.new(context, build_launcher)
        build_stage.set_name(_("Building project"))
        build_stage.connect('query', self._query)
        self.track(pipeline.attach(Ide.PipelinePhase.BUILD, 0, build_stage))

    def _query(self, stage, pipeline, targets, cancellable):
        stage.set_completed(False)
