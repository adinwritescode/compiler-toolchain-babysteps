parse:
	gcc -g predictive_parser.c -o parse -lm
post:
	gcc -g postfix.c -o post
clean:
	rm -f parse, post