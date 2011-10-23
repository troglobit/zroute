# Makefile for zroute, a tool for intracting with Zebra         -*-Makefile-*-

# VERSION       ?= $(shell git tag -l | tail -1)
VERSION      ?= 0.1.2
NAME          = zroute
EXEC          = $(NAME)
PKG           = $(NAME)-$(VERSION)
ARCHIVE       = $(PKG).tar.bz2

ROOTDIR      ?= $(dir $(shell pwd))
RM           ?= rm -f
CC           ?= $(CROSS)gcc

prefix       ?= /usr/local
sysconfdir   ?= /etc
datadir       = $(prefix)/share/doc/zroute
mandir        = $(prefix)/share/man/man8

CFLAGS       += -O2 -W -Wall -Werror
#CFLAGS      += -O -g
CPPFLAGS     += -DHAVE_INET_NTOP -DHAVE_SOCKLEN_T -DQUAGGA_NO_DEPRECATED_INTERFACES
CPPFLAGS     += -D'VERSION="$(VERSION)"'
LDLIBS        = -lzebra
OBJS          = zroute.o util.o

all: $(EXEC)

.c.o:
	@printf "  CC      $@\n"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(EXEC): $(OBJS)
	@printf "  LINK    $@\n"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

clean:
	-@$(RM) $(OBJS) $(EXEC)

distclean:
	-@$(RM) $(OBJS) core $(EXEC) *.BAK *~ *.o *.map .*.d *.out tags TAGS

dist: distclean
	@echo "Building bzip2 tarball of $(PKG) in parent dir..."
	git archive --format=tar --prefix=$(PKG)/ $(VERSION) | bzip2 >../$(ARCHIVE)
	@(cd ..; md5sum $(ARCHIVE) | tee $(ARCHIVE).md5)

tags: $(SRCS)
	@ctags $(SRCS)


