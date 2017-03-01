#Makefile for usercode

CC = gcc

CLAGS = 
LDFLAGS =

DMAEXEC = drawdma
FIFOEXEC = drawfifo

all: $(DMAEXEC) $(FIFOEXEC)

${DMAEXEC}: drawdma.o
	${CC} drawdma.c -lm -o ${DMAEXEC}

${FIFOEXEC}: drawfifo.o
	${CC} drawfifo.c -o ${FIFOEXEC}    
    
clean:
	rm -rf ${DMAEXEC}
	rm -rf ${FIFOEXEC}
	rm -rf *.swp
    rm -rf *.o
