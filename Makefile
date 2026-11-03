help:
	@echo "" 
	@echo " Internet Protocols Mini-Project - 2026" 
	@echo "             Makefile help             " 
	@echo "" 
	@echo " 1) Build one step; Usage: make step<number>"
	@echo " Example: make step1"
	@echo ""
	@echo " 2) Build all steps; Usage: make all"
	@echo ""
	@echo " 3) Remove built objects; Usage: make clean"
	@echo ""

step1: step1.c
	gcc -Wall -Wextra step1.c -o step1

step2: step2.c
	gcc -Wall -Wextra step2.c -o step2

step3: step3.c
	gcc -Wall -Wextra step3.c -o step3

step4: step4.c
	gcc -Wall -Wextra step4.c -o step4

all: step1 step2 step3 step4

clean:
	rm -f step1 step2 step3 step4

