
all: librm.a  myapp

librm.a:  rm.c
	gcc -Wall -c rm.c
	ar -cvq librm.a rm.o
	ranlib librm.a

myapp: myapp.c
	gcc -Wall -o myapp myapp.c -L. -lrm -lpthread

clean: 
	rm -fr *.o *.a *~ a.out  myapp rm.o rm.a librm.a

