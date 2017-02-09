#Makefile for usercode

CC = gcc

CLAGS = 
LDFLAGS =

EXEC = mycode

${EXEC}: mycode.o
    ${CC} mycode.c -o ${EXEC}
    
clean:
    rm -rf ${EXEC}
    rm -rf *.swp