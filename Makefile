fc: myshell.c
	gcc -I -Wall myshell.c -o myshell -lreadline

clean:
	rm myshell
