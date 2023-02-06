CC= gcc
CFLAGS= -Og -std=gnu99

ctok:
	$(CC) $(CFLAGS) ctok.c parser.c data.c constexpr.c metaprogramming.c code_validator.c -o compiler -lm -g

install: ctok
	cp ./ctok /usr/local/bin/

uninstall:
	rm /usr/local/bin/ctok

clean:
	rm -f *.exe *.out *.bin *.o compiler
