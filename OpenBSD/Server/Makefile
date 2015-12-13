INCLUDES += ${.CURDIR}/dependencies/http-parser

PROG=mirrord
SRCS=main.c

.PATH: ${.CURDIR}/dependencies/http-parser
SRCS+=http_parser.c

#.PATH: ${.CURDIR}/dependencies/logger
#SRCS+=http_parser.c 

CFLAGS+= -g 
#CFLAGS+= -DDEBUG
#CFLAGS+=   -Werror 
#CFLAGS+=	-Wall
#CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
#CFLAGS+=	-Wmissing-declarations
#CFLAGS+=	-Wshadow -Wpointer-arith
#CFLAGS+=	-Wsign-compare
LDADD=-lutil -levent
MAN=
.include <bsd.prog.mk> 