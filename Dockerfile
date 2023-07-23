FROM alpine:latest
RUN apk add build-base make
WORKDIR /ftp
COPY . .
RUN make
RUN mkdir /Documents
EXPOSE 2121
CMD ["/ftp/main"]
