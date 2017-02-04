ruler
=====

ruler is a program for creating window "rules" like in some window managers like
`i3` or `bspwm`.
The user writes the rules in a config file. The program executes commands when
windows with a specific set of properties are created. For example, all windows in the
`Firefox` class or with the `browser` role can be moved to workspace 2 with a command.

### Config file syntax

```
DESCRIPTOR
	[;]COMMAND

DESCRIPTOR := CRITERION_1=STRING_1 CRITERION_2=STRING_2 ...
CRITERION_i := class | instance | type | name | role
```

`STRING_i` is any string enclosed between double quotes (`"`).

`STRING_i` and `COMMAND` can contain escaped characters, preceded by a `\`. You
can have multi-line commands this way.

`STRING_i` can be a regular expression (POSIX extended regular expression).

If `COMMAND` is preceded by a `;`, the command will be run synchronously,
otherwise it will be run asynchronously.

`COMMAND` will be executed by the shell set in the `SHELL` environment
variable. The window id will be set in the `RULER_ID` environment variable.

`CRITERION` can be:

* `class` - the second part of `WM_CLASS` window property.
* `instance` - the first part of `WM_CLASS` window property.
* `type` - the `_NET_WM_WINDOW_TYPE` window property. `VALUE` can be
	`desktop`, `dock`, `toolbar`, `menu`, `utility`, `splash`, `dialog`,
	`dropdown_menu`, `popup_menu`, `tooltip`, `notification`, `combo`, `dnd`,
	`normal`.
* `name` - the X11 window title (`_NET_WM_NAME` and `WM_NAME` properties, the
	latter used as a fallback).
* `role` - window role (`WM_WINDOW_ROLE` property)

### Example

Assign all browsers to group 2.

```
role="browser"
	wtf "$RULER_WID" && waitron group_add_window 2
```

Implementation details
----------------------

* Ruler is written in POSIX C (define `_POSIX_C_SOURCE`)
* Ruler will make use of the XCB and XCB Util WM libraries.
* Regular expressions implementation is [POSIX](https://www.gnu.org/software/libc/manual/html_node/Regular-Expressions.html)
