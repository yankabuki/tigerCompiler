INCLUDE	= include
SRC		= src
OBJ		= obj
SOURCE	= $(shell cd src; printf "%s " *.c)
OBJS	= $(subst .c,.o,$(SOURCE))

%.o: $(SRC)/%.c
	gcc -g -O0 -I $(INCLUDE) -c -o $(OBJ)/$@ $<

tc: $(OBJS)
	cd $(OBJ); gcc -g -O0 -I $(INCLUDE) -o ../$@ $(OBJS)

clean:
	rm -f obj/*.* $(SRC)/y.tab.c $(SRC)/lex.yy.c $(INCLUDE)/y.tab.h y.output tc

all:
	cd $(SRC); lex tiger.lex
	bison -dv -o $(SRC)/y.tab.c $(SRC)/tiger.grm
	mv $(SRC)/y.tab.h $(INCLUDE)/y.tab.h
	mv $(SRC)/y.output y.output
	make tc
