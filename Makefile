CC = gcc
INCLUDE = .
SOURCES=dir_crawl.c dir_crawl_helper.c
OBJECTS=$(SOURCES:.c=.o)

all: dircrawl

dircrawl: dir_crawl.o dir_crawl_helper.o
	  $(CC) -pthread -g dir_crawl.o dir_crawl_helper.o -o $@ -I$(INCLUDE)

clean:
	rm -f $(OBJECTS) dircrawl
