ruler(1) -- A window rule daemon
================================

## SYNOPSIS

`ruler` [-hiop] [-s <shell>] <filename> [<filename>...]

## DESCRIPTION

`ruler` is an X daemon that executes arbitrary commands for windows with
specific windows, called *rules*.

## OPTIONS

* `-h`:
	Print usage and version information.

* `-i`:
	Ignore case in rule descriptors.

* `-o`:
	Apply rules on windows with *override_redirect* set, like panels and docks.

* `-p`:
	Apply rules when windows change their properties.

* `-s` <shell>:
	Execute rule commands with <shell>.

## BEHAVIOR

`ruler` is a daemon that listens to X window events and applies a set of rules
on windows that match them. A rule is made from two parts: a descriptor, that is
a set of properties that must match with the properties of a window, and a
command, that is piped to an interpreter (`$SHELL` by default).

`ruler` reads its configuration file from `$XDG_CONFIG_HOME/ruler/rulerrc` by
default, or from the command line if specified.

If `ruler` receives `SIGUSR1` or `SIGUSR2`, it will reload the specified
configuration files or pause rule interpreting respectively.

Commands are executed by piping to the interpreter. (like `echo "COMMAND" |
		$SHELL`). The chosen shell is by default `$SHELL`.

## CONFIGURATION

Each line of the configuration file is interpreted like so:

* If it starts with a `#`, it is ignored.

* If it is indented with tabs and/or spaces, it is interpreted as a command.

* Else, it's a descriptor.

A descriptor - command pair is called a *block*. Blocks can have newlines
between them.

Syntax:

```
DESCRIPTOR
	[;]COMMAND

DESCRIPTOR := CRITERION_1=STRING_1 CRITERION_2=STRING_2 ...
CRITERION_i := class | instance | type | name | role
```

`STRING_i` is any string enclosed between double quotes (`"`).

`STRING_i` and `COMMAND` can contain escaped characters, preceded by a `\`. You
can have multi-line commands this way.

`STRING_i` is a POSIX extended regular expression.

If `COMMAND` is preceded by a `;`, the command will be run synchronously,
otherwise it will be run asynchronously.

`COMMAND` will be executed by the shell set in the `SHELL` environment
variable. The window id will be set in the `RULER_ID` environment variable.

`CRITERION` can be:

* `class` - the second part of `WM_CLASS` window property.

* `instance` - the first part of `WM_CLASS` window property.

* `type` - the `_NET_WM_WINDOW_TYPE` window property. `VALUE` can be a
combination of
	`desktop`, `dock`, `toolbar`, `menu`, `utility`, `splash`, `dialog`,
	`dropdown_menu`, `popup_menu`, `tooltip`, `notification`, `combo`, `dnd`,
	`normal`, separated by commas.

	Example value: `combo,dnd`.

* `name` - the X11 window title (`_NET_WM_NAME` and `WM_NAME` properties, the
	latter used as a fallback).

* `role` - window role (`WM_WINDOW_ROLE` property)

## EXAMPLE

```c
# assign all browsers to group 2
role="browser"
	wtf "$RULER_WID" && waitron group_add_window 2

# say hello if a window is created, synchronously
name=".*"
	;echo "Hello!";\
		echo "How are you?"
```

## AUTHOR

Tudor Roman <tudurom at gmail dot com>

## SEE ALSO

wmutils(1), regex(7)
