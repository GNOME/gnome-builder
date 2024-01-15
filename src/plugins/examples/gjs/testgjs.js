import GObject from 'gi://GObject';
import Ide from 'gi://Ide';

// This is an IdeApplicationAddin. Application addins are created once for
// the application at startup. You can do things like handle command-line
// options (if X-At-Startup=true is set in your .plugin), respond to
// app-level changes, or any other type of singleton work you need.
export var TestApplicationAddin = GObject.registerClass({
    Implements: [Ide.ApplicationAddin],
}, class TestApplicationAddin extends GObject.Object {
    vfunc_load(app) {
        print('loading gjs plugin!');
    }

    vfunc_unload(app) {
        print('unloading gjs plugin!');
    }
});

// This is an IdeBufferAddin. Buffer addins have a new instance created
// for every IdeBuffer within the IDE.  An IdeBuffer represents the
// contents of a file while it is being edited by a GtkSourceView.
export var TestBufferAddin = GObject.registerClass({
    Implements: [Ide.BufferAddin],
}, class TestBufferAddin extends GObject.Object {

    vfunc_language_set(buffer, language_id) {
        print('language set to', language_id);
    }

    vfunc_file_loaded(buffer, file) {
        print(file.get_uri(), 'loaded');
    }

    vfunc_save_file(buffer, file) {
        print('before saving buffer to', file.get_uri());
    }

    vfunc_file_saved(buffer, file) {
        print('after buffer saved to', file.get_uri());
    }

    vfunc_change_settled(buffer) {
        print('spurious changes have settled');
    }

    vfunc_load(buffer) {
        print('load buffer addin');
    }

    vfunc_unload(buffer) {
        print('unload buffer addin');
    }

    vfunc_style_scheme_changed(buffer) {
        let scheme = buffer.get_style_scheme();
        print('style scheme changed to', scheme ? scheme.get_id() : scheme);
    }
});
