# Libtheft - this is used for testing.
# Please visit: https://github.com/silentbicycle/theft
# clone & `sudo make install` will get it working
LIBTHEFT_BASEPATH := /usr/local
LIBTHEFT_INCPATH := $(LIBTHEFT_BASEPATH)/include
LIBTHEFT_LIBPATH := $(LIBTHEFT_BASEPATH)/lib

CHECK ?= 0

CPPFLAGS := -Wall -Werror 
ifeq ($(CHECK),1)
CFLAGS := -ggdb -fsanitize=address -O3
else
CPPFLAGS += -DNDEBUG
CFLAGS := -O3
endif

CSRCS := trie.c

all:: trie.o
	$(info set CHECK=1 when compiling to enable asserts and sanitizers)
	$(info "make test" will build the tests)

test:: trie.proptest.exe trie.unittest.exe

.SECONDEXPANSION:

%.proptest.exe: %.proptest.c $$*.o
	gcc $(CPPFLAGS) -I$(LIBTHEFT_INCPATH) $(CFLAGS) $^ -o $@ -L$(LIBTHEFT_LIBPATH) -ltheft

%.unittest.exe: %.unittest.c $$*.o
	gcc $(CPPFLAGS) $(CFLAGS) $^ -o $@
	
clean:
	rm -f *.o *.exe
