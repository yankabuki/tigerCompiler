%{
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "y.tab.h"
#include "errormsg.h"


int charPos;

int yywrap(void)
{
 charPos=1;
 return 1;
}


void adjust()
{
 EM_tokPos=charPos;
 charPos+=yyleng;
}

%}

DIGIT  [0-9]
LETTER ([a-z]|[A-Z])
ALPHA  ({LETTER}|{DIGIT})


%x  COMMENT
%%

"/*"          {adjust(); BEGIN COMMENT;}
<COMMENT>\n   {adjust(); EM_newline(); continue;}
<COMMENT>"*/" {adjust(); BEGIN 0;}
<COMMENT>.

[ \t\r]     {adjust(); continue;}
"."         {adjust(); return DOT;}
\n          {adjust(); EM_newline();}
for         {adjust(); return FOR;}
array       {adjust(); return ARRAY;}
if          {adjust(); return IF;}
then        {adjust(); return THEN;}
else        {adjust(); return ELSE;}
while       {adjust(); return WHILE;}
to          {adjust(); return TO;}
do          {adjust(); return DO;}
let         {adjust(); return LET;}
in          {adjust(); return IN;}
end         {adjust(); return END;}
of          {adjust(); return OF;}
break       {adjust(); return BREAK;}
nil         {adjust(); return NIL;}
function    {adjust(); return FUNCTION;}
var         {adjust(); return VAR;}
type        {adjust(); return TYPE;}
import      {adjust(); return IMPORT;}
primitive   {adjust(); return PRIMITIVE;}
","         {adjust(); return COMMA;}
":"         {adjust(); return COLON;}
";"         {adjust(); return SEMICOLON;} 
"("         {adjust(); return LPAREN;}
")"         {adjust(); return RPAREN;}
"["         {adjust(); return LBRACK;}
"]"         {adjust(); return RBRACK;}
"{"         {adjust(); return LBRACE;}
"}"         {adjust(); return RBRACE;}
"+"         {adjust(); yylval.sval = String(yytext); return PLUS;}
"-"         {adjust(); yylval.sval = String(yytext); return MINUS;}
"*"         {adjust(); yylval.sval = String(yytext); return TIMES;}
"/"         {adjust(); yylval.sval = String(yytext); return DIVIDE;}
"="         {adjust(); yylval.sval = String(yytext); return EQ;}
"<>"        {adjust(); yylval.sval = String(yytext); return NEQ;}
"<"         {adjust(); yylval.sval = String(yytext); return LT;}
"<="        {adjust(); yylval.sval = String(yytext); return LE;}
">"         {adjust(); yylval.sval = String(yytext); return GT;}
">="        {adjust(); yylval.sval = String(yytext); return GE;}
"&"         {adjust();yylval.sval = String(yytext); return AND;}
"|"         {adjust();yylval.sval = String(yytext); return OR;}
":="        {adjust();yylval.sval = String(yytext); return ASSIGN;}

{DIGIT}+             	{adjust(); yylval.ival = atoi(yytext); return INT;}
{LETTER}({ALPHA}|_)*	{adjust(); yylval.sval = String(yytext); return ID;} 
L?\"(\\.|[^\\"])*\"	 	{ adjust(); yylval.sval = String(yytext); return STRING;}
.                    	{ adjust(); EM_error(EM_tokPos,"illegal token"); }
<<EOF>>              	{ yyterminate(); }
%%

