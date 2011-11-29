SRC=ktacmd.c

ktacmd:	$(SRC)
	gcc -Wall -Werror -o $@ $(SRC)

clean:
	@ rm -rf ktacmd
