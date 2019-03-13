from gi.repository import Ide
from gi.repository import Gio
from gi.repository import GObject

class BuildStreamBuildSystemDiscovery(Ide.SimpleBuildSystemDiscovery):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        print('find bst file............................\n')
        self.props.glob = 'project.conf'
        self.props.hint = 'buildstream_plugin'
        self.props.priority = 4000

class BuildStreamBuildSystem(Ide.Object, Ide.BuildSystem):
    project_file = GObject.Property(type=Gio.File)

    def do_get_id(self):
        return 'buildstream'

    def do_get_display(self):
        return 'BuildStream'

    def do_get_priority(self):
        return 2000

class BuildStreamPipelineAddin(Ide.Object, Ide.PipelineAddin):

    def do_load(self, pipeline):

        context = self.get_context()

        print(context)

        build_system = Ide.BuildSystem.from_context(context)

        if type(build_system) != BuildStreamBuildSystem:
            return

        build_launcher = pipeline.create_launcher()
        build_launcher.set_cwd(srcdir)
        build_launcher.push_argv("bst")
        build_launcher.push_argv('compile')
