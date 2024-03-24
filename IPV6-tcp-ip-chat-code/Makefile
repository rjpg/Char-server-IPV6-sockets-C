include Make.defines

#PROGS =	chatcli 
PROGS =		chatcli chatsrv

all:	${PROGS}

chatcli:	chatcli.o
		${CC} ${CFLAGS} -o $@ chatcli.o ${LIBS}

chatsrv:	chatsrv.o
		${CC} ${CFLAGS} -o $@ chatsrv.o ${LIBS}

clean:
		rm -f ${PROGS} ${CLEANFILES}
