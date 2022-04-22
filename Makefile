all: server client

server: src/server.c src/common.c src/error_handlers.c
	gcc $^ -Iinclude -std=gnu11 -o $@

client: src/client.c src/common.c src/error_handlers.c
	gcc $^ -Iinclude -std=gnu11 -o $@

.PHONY: clean
clean:
	@echo "clean project"
	-rm server client
	@echo "clean completed"



