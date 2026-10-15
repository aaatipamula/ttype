CC := gcc
CFLAGS := -g -Wall -Wextra -Werror -fsanitize=address
RELEASE_CFLAGS := -O2 -Wall -Wextra -Werror
LIBS := -lncurses
EXE := ttype
LOG := stderr.log

PREFIX ?= ~/.local
BINDIR := $(PREFIX)/bin

CONFIG_HOME := $(shell echo $${XDG_CONFIG_HOME:-$$HOME/.config})
WORDBANK_SRC := wordbank.txt
WORDBANK_DST := $(CONFIG_HOME)/ttype/wordbank.txt

.PHONY: build release run sane install wordbank

build: sane
	$(CC) $(CFLAGS) main.c -o $(EXE) $(LIBS)

release:
	$(CC) $(RELEASE_CFLAGS) main.c -o $(EXE) $(LIBS)

install: release wordbank
	mkdir -p $(BINDIR)
	install -m755 $(EXE) $(BINDIR)/$(EXE)

# Seeds the default wordbank location, without clobbering an existing file
wordbank:
	@if [ ! -f $(WORDBANK_DST) ]; then \
		mkdir -p $(dir $(WORDBANK_DST)); \
		install -m644 $(WORDBANK_SRC) $(WORDBANK_DST); \
	fi

# Restore the terminal in case an earlier run died before endwin()
sane:
	@stty sane 2>/dev/null || true

# On a crash: restore the terminal, leave the curses screen, then show stderr
run: build wordbank
	./$(EXE) 2> $(LOG) || { status=$$?; stty sane; tput rmcup; cat $(LOG) >&2; exit $$status; }
