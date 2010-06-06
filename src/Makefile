OBJS=${OBJSPATH}/buffer.o ${OBJSPATH}/log.o  ${OBJSPATH}/chunk.o ${OBJSPATH}/connection.o ${OBJSPATH}/array.o\
	 ${OBJSPATH}/configure.o ${OBJSPATH}/fdevent.o ${OBJSPATH}/threadpool.o ${OBJSPATH}/error_page.o\
	 ${OBJSPATH}/fde_epoll.o ${OBJSPATH}/joblist.o ${OBJSPATH}/main.o ${OBJSPATH}/network.o\
	 ${OBJSPATH}/base.o ${OBJSPATH}/request.o ${OBJSPATH}/plugin.o ${OBJSPATH}/response.o ${OBJSPATH}/network_write.o\
	 ${OBJSPATH}/data_string.o ${OBJSPATH}/data_integer.o ${OBJSPATH}/data_array.o ${OBJSPATH}/keyvalue.o

FLAGS=-ggdb -D HAVE_SYS_EPOLL_H

OBJSPATH=../objects
CC=gcc


swiftd: ${OBJS} ${OBJSPATH}/mod_dir_index.o dir_index.so
	${CC}  ${FLAGS} ${OBJS} -o swiftd -lpthread -ldl

${OBJSPATH}/buffer.o: buffer.c
	${CC} -c ${FLAGS} buffer.c -o ${OBJSPATH}/buffer.o
${OBJSPATH}/chunk.o: chunk.c
	${CC} -c ${FLAGS} chunk.c -o  ${OBJSPATH}/chunk.o
${OBJSPATH}/connection.o:connection.c
	${CC} -c ${FLAGS} connection.c  -o  ${OBJSPATH}/connection.o
${OBJSPATH}/array.o: array.c
	${CC} -c ${FLAGS} array.c  -o  ${OBJSPATH}/array.o
${OBJSPATH}/data_array.o:data_array.c
	${CC} -c ${FLAGS} data_array.c  -o ${OBJSPATH}/data_array.o
${OBJSPATH}/data_integer.o:data_integer.c
	${CC} -c ${FLAGS} data_integer.c  -o ${OBJSPATH}/data_integer.o
${OBJSPATH}/data_string.o:data_string.c
	${CC} -c ${FLAGS} data_string.c  -o ${OBJSPATH}/data_string.o
${OBJSPATH}/log.o:log.c
	${CC} -c ${FLAGS} log.c  -o  ${OBJSPATH}/log.o
${OBJSPATH}/configure.o:configure.c
	${CC} -c ${FLAGS} configure.c  -o ${OBJSPATH}/configure.o
${OBJSPATH}/fdevent.o:fdevent.c
	${CC} -c ${FLAGS} fdevent.c  -o ${OBJSPATH}/fdevent.o
${OBJSPATH}/threadpool.o:threadpool.c
	${CC} -c ${FLAGS} threadpool.c -o ${OBJSPATH}/threadpool.o 
${OBJSPATH}/fde_epoll.o:fde_epoll.c
	${CC} -c ${FLAGS} fde_epoll.c -o  ${OBJSPATH}/fde_epoll.o
${OBJSPATH}/joblist.o:joblist.c
	${CC} -c ${FLAGS} joblist.c -o ${OBJSPATH}/joblist.o
${OBJSPATH}/main.o:main.c
	${CC} -c ${FLAGS} main.c -o ${OBJSPATH}/main.o
${OBJSPATH}/network.o:network.c
	${CC} -c ${FLAGS} network.c -o  ${OBJSPATH}/network.o
${OBJSPATH}/base.o:base.c
	${CC} -c ${FLAGS} base.c -o  ${OBJSPATH}/base.o
${OBJSPATH}/request.o:request.c
	${CC} -c ${FLAGS} request.c  -o  ${OBJSPATH}/request.o
${OBJSPATH}/keyvalue.o:keyvalue.c
	${CC} -c ${FLAGS} keyvalue.c -o ${OBJSPATH}/keyvalue.o
${OBJSPATH}/plugin.o:plugin.c
	${CC} -c ${FLAGS} plugin.c  -o  ${OBJSPATH}/plugin.o
${OBJSPATH}/response.o:response.c
	${CC} -c ${FLAGS} response.c  -o  ${OBJSPATH}/response.o
${OBJSPATH}/error_page.o:error_page.c
	${CC} -c ${FLAGS} error_page.c  -o  ${OBJSPATH}/error_page.o
${OBJSPATH}/network_write.o:network_write.c
	${CC} -c ${FLAGS} network_write.c  -o  ${OBJSPATH}/network_write.o
${OBJSPATH}/mod_dir_index.o:mod_dir_index.c
	${CC} -c ${FLAGS} mod_dir_index.c  -o  ${OBJSPATH}/mod_dir_index.o

dir_index.so: ${OBJS} ${OBJSPATH}/mod_dir_index.o
	${CC} -fPIC -shared ${OBJS} ${OBJSPATH}/mod_dir_index.o -o dir_index.so
	mv dir_index.so /home/hcy/tmp/swiftd/plugins/

clean:
	rm ${OBJSPATH}/*.o
#	${CC} -c ${FLAGS}   -o 
