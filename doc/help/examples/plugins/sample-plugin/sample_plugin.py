# This file is public domain.


from gi.repository import Builder
from gi.repository import Ide
from gi.repository import GObject
from gi.repository import GtkSource

class ApplicationAddin(GObject.Object, Builder.ApplicationAddin):
    """
    This addin class will be loaded once per application.
    It is loaded when the application loads, or when the plugin is activated.
    It is common to use an ApplicationAddin for singleton type features.
    """

    def do_load(self, app):
        """
        Perform startup features.

        If using this class as a singleton, you might want to register a
        class property to access the instance, such as:

        >>> ApplicationAddin.instance = self
        """
        print("My addin has loaded!")

    def do_unload(self, app):
        """
        Unload routine.

        Possibly cleanup your singleton instance if you did that.
        >>> ApplicationAddin.instance = None
        """
        print("My addin has unloaded")

class WorkbenchAddin(GObject.Object, Builder.WorkbenchAddin):
    """
    This addin class will be loaded once per "Project Window".

    The project window is represented by a "Workbench", and it will
    have exactly one LibIDE context associated with it (found using
    workbench.props.context property.

    Use this plugin if you need to do something once-per-project window.
    """

    def do_load(self, workbench):
        pass

    def do_unload(self, workbench):
        pass

class EditorViewAddin(GObject.Object, Builder.EditorViewAddin):
    """
    This addin will be loaded once for every editor in Builder.
    If you need to do something that is per-editor, this is the class for you.
    """

    def do_load(self, editor):
        pass

    def do_unload(self, editor):
        pass

class CompletionProvider(Ide.Object, GtkSource.CompletionProvider, Ide.CompletionProvider):
    # See GtkSource.CompletionProvider for all the things you can do.

    # NOTE: You must set X-Completion-Provider-Languages in .plugin file!

    def do_populate(self, context):
        iter = context.props.iter

        # only add our items after a `.'
        if iter.backward_char() and iter.get_char() == '.':
            item = GtkSource.CompletionItem(label='do_something()', text='do_something()')
            context.add_proposals(self, [item], True)

class SampleDevice(Ide.Device):
    def __init__(self):
        self.set_id('my-sample-id')
        self.set_display_name('Sample Device')

    def do_get_system_type(self):
        return 'x86_64-linux-gnu'

class DeviceProvider(Ide.Object, Ide.DeviceProvider):
    """
    This IdeDeviceProvider interface allows plugins to provide devices to
    Builder. This will be expanded to support cross-copmiles, deployment, and
    other hardware specific features.
    """

    # Settled means we have finished discovering devices.
    settled = GObject.Property(type=GObject.TYPE_BOOLEAN)

    def __init__(self, *args, **kwargs):
        device = SampleDevice()
        self.emit_device_added(device)

        self.settled = True
        self.notify("settled")

