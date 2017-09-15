project_name = client_win
sources = main.c

$(project_name): $(sources)
	gcc $< -o $@ -Wall -Wextra -lws2_32 -DWINVER=0x0501

.PHONY: clean
clean:
	rm -f $(project_name)

