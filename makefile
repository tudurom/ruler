NAME = ruler

CFLAGS += -Wall -g

ruler: ruler.c parser scanner
	clang *.c -o ruler

parser: parser.y
	yacc parser.y

scanner: scanner.l
	lex scanner.l

clean:
	rm $(NAME) lex.yy.c y.tab.c y.tab.h
