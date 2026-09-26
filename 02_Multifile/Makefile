GENERATES = prog prog-a prog-so README liboutput.so liboutput_static.a prog-def0.out prog-a0.out prog-so0.out  prog-def1.out prog-a1.out prog-so1.out prog-def3.out prog-a3.out prog-so3.out
TRASH = *.o *~ o.*

%.o:		%.c
		cc $< -fPIC -c -o $@

liboutput_static.a:	fun.o const.o
		ar -rcs $@ $^

liboutput.so:	fun.o const.o
		cc -shared -o $@ $^

all:	README prog prog-a prog-so

prog: 	const.o fun.o prog.o
		cc $^ -o $@

prog-a: 	 prog.o liboutput_static.a
		cc $^ -o $@

prog-so:  prog.o liboutput.so
		cc $^ -o $@ -L. -loutput

fun.o:  	outlib.h

test:
		./prog prog > prog-def0.out 2>&1
		./prog-a prog > prog-a0.out 2>&1
		LD_LIBRARY_PATH=. ./prog-so prog > prog-so0.out 2>&1
		cmp prog-def0.out prog-a0.out
		cmp prog-a0.out prog-so0.out

		./prog 1 > prog-def1.out 2>&1
		./prog-a 1 > prog-a1.out 2>&1
		LD_LIBRARY_PATH=. ./prog-so 1 > prog-so1.out 2>&1
		cmp prog-def1.out prog-a1.out
		cmp prog-a1.out prog-so1.out

		./prog 1 2 3 > prog-def3.out 2>&1
		./prog-a 1 2 3 > prog-a3.out 2>&1
		LD_LIBRARY_PATH=. ./prog-so 1 2 3 > prog-so3.out 2>&1
		cmp prog-def3.out prog-a3.out
		cmp prog-a3.out prog-so3.out

README:	prog
		./$< 2> $@

clean:
		rm -rf $(TRASH) $(GENERATES)


