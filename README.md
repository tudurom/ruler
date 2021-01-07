ruler
=====

ruler is a program used for creating rules like in some window managers, like
[i3](https://i3wm.org/) or [bspwm](https://github.com/baskerville/bspwm/).

Rules are commands that are associated with a set of window properties
(*descriptors*). When a
window is created, `ruler` executes all rules whose descriptors match the properties of a
window.

Descriptors can be defined as regular expressions (POSIX Extended Regular
Expressions) to avoid code repetition.

Commands are executed asynchronously by default. If a command is prefixed with a
semicolon, it will be run synchronously.

For more information, see the included manual page (`ruler(1)`).

Example configuration
---------------------

```
# move all browsers to workspace 2
role="browser"
	wtf "$RULER_ID" && waitron group_add_window 2

# drop a notification if a window containing that fifth glyph is born
instance=".*e.*"
	echo "warning!" > /tmp/notifyd.fifo
```

Dependencies
------------

* [xcb](https://xcb.freedesktop.org/)
* [xcb-util-wm](https://www.archlinux.org/packages/extra/x86_64/xcb-util-wm/)
* [libwm](https://github.com/wmutils/libwm)

Build time dependencies:

* a yacc implementation (GNU bison, OpenBSD yacc etc.)
* a lex implementation (flex)

Building and installing
-----------------------

```
$ make
# make install
```

The `Makefile` respects the `DESTDIR` and `PREFIX` environment variables.
