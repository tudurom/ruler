NAME = ruler

CFLAGS += -Wall -O2
LDFLAGS += -lxcb -lxcb-icccm -lxcb-ewmh -lwm

ruler: ruler.c parser scanner
	clang *.c $(CFLAGS) $(LDFLAGS) -o ruler

parser: parser.y
	yacc parser.y

scanner: scanner.l
	lex scanner.l

clean:
	rm $(NAME) lex.yy.c y.tab.c y.tab.h
