CFLAGS=-g
LINKFLAGS=
OBJFILES=473_mm.o test-code1.o test-code2.o test-code3.o test-code4.o test-code5.o test-code6.o
OUTPUT_FILES=results.txt test1.txt test2.txt test3.txt test4.txt test5.txt test6.txt
EXECUTABLES=test-code1.exe test-code2.exe test-code3.exe test-code4.exe test-code5.exe test-code6.exe

all: $(EXECUTABLES)

test: $(EXECUTABLES)
	./test-code1.exe >./test1.txt; diff test1.txt output_1.txt || echo '\033[1;31mFailed on test 1 \033[00m'
	./test-code2.exe >./test2.txt; diff test2.txt output_2.txt || echo '\033[1;31mFailed on test 2 \033[00m'
	./test-code3.exe >./test3.txt; diff test3.txt output_3.txt || echo '\033[1;31mFailed on test 3 \033[00m'
	./test-code4.exe >./test4.txt; diff test4.txt output_4.txt || echo '\033[1;31mFailed on test 4 \033[00m'
	./test-code5.exe >./test5.txt; diff test5.txt output_5.txt || echo '\033[1;31mFailed on test 5 \033[00m'
	./test-code6.exe >./test6.txt; diff test6.txt output_6.txt || echo '\033[1;31mFailed on test 6 \033[00m'

%.exe: %.o 473_mm.o 473_mm.h
	gcc -o $@ $< 473_mm.o

clean:
	rm -f $(OBJFILES) $(OUTPUT_FILES) $(EXECUTABLES)

%.o: %.c 473_mm.h
	gcc -c $(CFLAGS) -o $@ $<

package:
	tar -cvzf Burlew-Hayes.tgz 473_mm.c 473_mm.h proj-description.txt
