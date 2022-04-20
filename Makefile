all: server client

server: src/server.c src/common.c src/error_handlers.c
	gcc $^ -Iinclude -o $@

client: src/client.c src/common.c src/error_handlers.c
	gcc $^ -Iinclude -o $@

.PHONY: clean
clean:
	@echo "clean project"
	-rm server client
	@echo "clean completed"



