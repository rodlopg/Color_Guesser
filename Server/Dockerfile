FROM gcc:latest
COPY . /usr/src/server
WORKDIR /usr/src/server
RUN gcc tcpServer.c -o tcpserver -lm
EXPOSE 5000
CMD ["./tcpserver"]