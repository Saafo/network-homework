all: tcp_echo_cli.out tcp_echo_srv.out

tcp_echo_cli.out: tcp_echo_cli.c
	gcc -g tcp_echo_cli.c -o tcp_echo_cli.out

tcp_echo_srv.out: tcp_echo_srv.c
	gcc -g tcp_echo_srv.c -o tcp_echo_srv.out

.PHONY: clean tmp
clean:
	rm *.out
tmp:
	rm stu_*.txt &&	for i in `netstat -anp | grep 30000 | grep tcp | awk '{print $$7}' | awk -F/ '{print $$1}' | grep -E [0-9]+`; do kill $$i; done