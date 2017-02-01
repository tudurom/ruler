%{
#include <ctype.h>

#define YYSTYPE char *
#include "y.tab.h"

static void skip_comment(void);
static void skip_blank(void);
%}

%%
("class"|"name")                  yylval = strdup(yytext); return CRITERION;
=                                 return EQUALS;
\"([^\"\\]*(\\.[^\"\\]*)*)\"      yylval = strdup(yytext); return STRING;
^[ \t]+([^\r\n\t\f ](([^\n\\])|(\\(.|\n)))+)  yylval=strdup(yytext); return COMMAND;
\n                                return NEWLINE;
[[:space:]]                       skip_blank();
<<EOF>>                           return END;
"#".* skip_comment();
%%

static void
skip_comment(void)
{
	int c;

	c = input();
	while (c != '\n' && c != EOF)
		c = input();

	if (c == EOF)
		unput(c);
}

static void
skip_blank(void)
{
	int c;

	c = input();
	while (isspace(c) && c != EOF)
		c = input();

	unput(c);
}
