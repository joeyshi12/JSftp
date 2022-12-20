
#This is a hack to pass arguments to the run command and probably only
#works with gnu make.
ifeq (run,$(firstword $(MAKECMDGOALS)))
  # use the rest as arguments for "run"
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  # ...and turn them into do-nothing targets
  $(eval $(RUN_ARGS):;@:)
endif
ifeq (test,$(firstword $(MAKECMDGOALS)))
  # use the rest as arguments for "run"
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  # ...and turn them into do-nothing targets
  $(eval $(RUN_ARGS):;@:)
endif


all: JSftp

#The following lines contain the generic build options
CC=gcc
CPPFLAGS=
CFLAGS=-g -Werror-implicit-function-declaration
CLIBS = -pthread

#List all the .o files here that need to be linked
OBJS=JSftp.o usage.o dir.o tcpserver.o ftpservice.o

usage.o: usage.c usage.h

dir.o: dir.c dir.h

tcpserver.o: tcpserver.c tcpserver.h

ftpservice.o: ftpservice.c ftpservice.h tcpserver.h dir.h

JSftp.o: JSftp.c usage.h ftpservice.h

JSftp: $(OBJS)
	$(CC) -o JSftp $(OBJS) $(CLIBS)

clean:
	rm -f *.o
	rm -f JSftp

.PHONY: run test
run: JSftp
	./JSftp $(RUN_ARGS)

test: test/test_csftp.py
	mkdir -p test/out
	./test/test_csftp.py $(RUN_ARGS)
