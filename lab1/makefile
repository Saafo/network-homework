all: tcp_echo_cli.out tcp_echo_srv.out

tcp_echo_cli.out: tcp_echo_cli.c
	gcc tcp_echo_cli.c -o tcp_echo_cli.out

tcp_echo_srv.out: tcp_echo_srv.c
	gcc tcp_echo_srv.c -o tcp_echo_srv.out

.PHONY: clean
clean:
	rm *.out