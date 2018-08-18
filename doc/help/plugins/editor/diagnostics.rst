#######################
Diagnostics and Fix-Its
#######################

In order to show diagnostics in the editor, you need to implemenet an 
`Ide.DiagnosticProvider` and override the `do_diagnose_async` and 
`do_diagnose_finish` methods.

The `do_diagnose_async` is an asynchronous method that will be called with a 
callback as the fifth parameter. The callback can be passed to a `Gio.Task` for
easy handling. When the task is done, `do_diagnose_finish` will be called with 
the `Gio.Task` object and is expected to return an `Ide.Diagnostics` object.