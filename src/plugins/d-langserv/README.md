# a quick langserver plugin for d

Meant to wrap [Pure-D's](https://github.com/Pure-D/serve-d)
`serve-d`. It's a straight copy & paste of the rust-langserver
implementation.

## Installing and testing

1. Install `dmd`. Its tooling is stable, 'apt install' or 'yum install' will be fine.
2. Set up your go environment-
```
dub fetch serve-d
dub build serve-d
```

## Bugs:
* I have to disable the rust-langserver extension, or else it seems to take
  precedence