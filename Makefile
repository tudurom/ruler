include config.mk

NAME = ruler

all: $(NAME)

$(NAME): ruler.c lex.yy.c y.tab.c
	clang $^ $(CFLAGS) $(LDFLAGS) -o ruler

%.tab.c %.tab.h: parser.y
	yacc $<

lex.yy.c: scanner.l
	lex $<

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install $(NAME) $(DESTDIR)$(PREFIX)/bin/$(NAME)
	cd ./man; $(MAKE) install

uninstall:
	rm -f $(NAME) $(DESTDIR)$(PREFIX)/bin/$(NAME)
	cd ./man; $(MAKE) uninstall

clean:
	rm $(NAME) lex.yy.c y.tab.c y.tab.h
