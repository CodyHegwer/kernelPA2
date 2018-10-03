all: multi-lookup.c util.c
	gcc -pthread -Wall -Wextra -o multi-lookup multi-lookup.c util.c
