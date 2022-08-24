# Notes

## Improvements

If I were doing this again and had more time than I did for this prototype, I'd
probably investigate a few more things.

 - Instead of having settings/object/action-name/action-target/etc I would abstract
   something to read/write values which can notify when there are changes. Something
   akin to a bi-directional GtkExpression.
 - I'd probably look for ways to use widgets natively within the XML file and create
   them on demand. That becomes problematic when doing bindings, but is still possible
   if we injected the right state into a new GtkBuilder XML starting from somesort
   of synthetic "child" element handled by the GtkBuildableIface.

