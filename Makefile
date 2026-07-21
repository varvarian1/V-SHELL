CC = gcc

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
TARGET = v-shell

CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lreadline -lhistory

ifdef DEBUG
	CFLAGS = -Wall -Wextra -g -O0 -DDEBUG -fsanitize=address
	LDFLAGS += -fsanitize=address
endif

SRCS = src/main.c \
       src/lexer.c \
	   src/parser.c \
	   src/executor.c \
	   src/prompt.c	\
	   src/symbol.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

install: $(TARGET)
	install -D $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm -f $(BINDIR)/$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

rebuild:
	make clean
	make all

.PHONY: all install uninstall clean rebuild
