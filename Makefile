CC = clang
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic
DB_PATH ?= data/tiny.db
tinydb: main.c
	$(CC) $(CFLAGS) $< -o $@
run: tinydb
	mkdir -p "$$(dirname "$(DB_PATH)")"
	./tinydb "$(DB_PATH)"
clean:
	$(RM) tinydb
.PHONY: run clean
