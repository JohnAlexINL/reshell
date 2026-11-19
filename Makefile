CC=gcc
FLAGS=-Os -s

all: clean
	bytes LICENSE LICENSE __LICENSE.h
	bytes REPLAY_HTML replay.html __replay.html.h
	$(CC) -o reshell reshell.c $(FLAGS)

.SILENT:clean
clean:
	@rm -f $(BIN) *.txt session.* __* 2>/dev/null
