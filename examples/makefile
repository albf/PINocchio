SOURCES = $(wildcard *.c)
EXAMPLES = $(patsubst %.c,%,$(SOURCES))
CFLAGS = -lpthread

all: $(EXAMPLES)

%: %.c
	$(CC) -o $@ $< $(CFLAGS)

clean: 
	rm -f $(EXAMPLES)
