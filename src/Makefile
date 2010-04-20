OBJS=${OBJSPATH}/buffer.o ${OBJSPATH}/log.o  ${OBJSPATH}/chunk.o ${OBJSPATH}/connection.o ${OBJSPATH}/array.o\
	 ${OBJSPATH}/configure.o ${OBJSPATH}/fdevent.o ${OBJSPATH}/threadpool.o ${OBJSPATH}/error_page.o\
	 ${OBJSPATH}/fde_epoll.o ${OBJSPATH}/joblist.o ${OBJSPATH}/main.o ${OBJSPATH}/network.o\
	 ${OBJSPATH}/base.o ${OBJSPATH}/request.o ${OBJSPATH}/plugin.o ${OBJSPATH}/response.o ${OBJSPATH}/network_write.o\
	 ${OBJSPATH}/data_string.o ${OBJSPATH}/data_integer.o ${OBJSPATH}/data_array.o ${OBJSPATH}/keyvalue.o
FLAGS=-g
OBJSPATH=../objects


swiftd: ${OBJS}
	gcc  ${FLAGS} ${OBJS} -o swiftd -lpthread

${OBJSPATH}/buffer.o: buffer.c
	gcc -c ${FLAGS} buffer.c -o ${OBJSPATH}/buffer.o
${OBJSPATH}/chunk.o: chunk.c
	gcc -c ${FLAGS} chunk.c -o  ${OBJSPATH}/chunk.o
${OBJSPATH}/connection.o:connection.c
	gcc -c ${FLAGS} connection.c  -o  ${OBJSPATH}/connection.o
${OBJSPATH}/array.o: array.c
	gcc -c ${FLAGS} array.c  -o  ${OBJSPATH}/array.o
${OBJSPATH}/data_array.o:data_array.c
	gcc -c ${FLAGS} data_array.c  -o ${OBJSPATH}/data_array.o
${OBJSPATH}/data_integer.o:data_integer.c
	gcc -c ${FLAGS} data_integer.c  -o ${OBJSPATH}/data_integer.o
${OBJSPATH}/data_string.o:data_string.c
	gcc -c ${FLAGS} data_string.c  -o ${OBJSPATH}/data_string.o
${OBJSPATH}/log.o:log.c
	gcc -c ${FLAGS} log.c  -o  ${OBJSPATH}/log.o
${OBJSPATH}/configure.o:configure.c
	gcc -c ${FLAGS} configure.c  -o ${OBJSPATH}/configure.o
${OBJSPATH}/fdevent.o:fdevent.c
	gcc -c ${FLAGS} fdevent.c  -o ${OBJSPATH}/fdevent.o
${OBJSPATH}/threadpool.o:threadpool.c
	gcc -c ${FLAGS} threadpool.c -o ${OBJSPATH}/threadpool.o -lphtread 
${OBJSPATH}/fde_epoll.o:fde_epoll.c
	gcc -c ${FLAGS} fde_epoll.c -o  ${OBJSPATH}/fde_epoll.o
${OBJSPATH}/joblist.o:joblist.c
	gcc -c ${FLAGS} joblist.c -o ${OBJSPATH}/joblist.o
${OBJSPATH}/main.o:main.c
	gcc -c ${FLAGS} main.c -o ${OBJSPATH}/main.o
${OBJSPATH}/network.o:network.c
	gcc -c ${FLAGS} network.c -o  ${OBJSPATH}/network.o
${OBJSPATH}/base.o:base.c
	gcc -c ${FLAGS} base.c -o  ${OBJSPATH}/base.o
${OBJSPATH}/request.o:request.c
	gcc -c ${FLAGS} request.c  -o  ${OBJSPATH}/request.o
${OBJSPATH}/keyvalue.o:keyvalue.c
	gcc -c ${FLAGS} keyvalue.c -o ${OBJSPATH}/keyvalue.o
${OBJSPATH}/plugin.o:plugin.c
	gcc -c ${FLAGS} plugin.c  -o  ${OBJSPATH}/plugin.o
${OBJSPATH}/response.o:response.c
	gcc -c ${FLAGS} response.c  -o  ${OBJSPATH}/response.o
${OBJSPATH}/error_page.o:error_page.c
	gcc -c ${FLAGS} error_page.c  -o  ${OBJSPATH}/error_page.o
${OBJSPATH}/network_write.o:network_write.c
	gcc -c ${FLAGS} network_write.c  -o  ${OBJSPATH}/network_write.o


clean:
	rm ${OBJSPATH}/*.o
#	gcc -c ${FLAGS}   -o 
