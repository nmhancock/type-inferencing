.PHONY: format cloc
all: main.c inference.c printer.c context.c inference.h
	$(CC) $(CFLAGS) -Wall -g -o main inference.c main.c printer.c context.c -flto
format:
	clang-format -i *.c *.h
cloc:
	cloc --by-file ./src
