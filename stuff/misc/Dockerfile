# Build `docker build -f stuff/misc/Dockerfile . -t xyz/quake2`
# Run `docker run -v ~/.yq2:/yquake2/.yq2 -p 27910:27910/udp xyz/quake2`
FROM ubuntu:24.04 AS build

WORKDIR /yquake2
RUN apt-get update
RUN apt-get install libcurl4-openssl-dev build-essential -y
COPY . .

RUN make server game

FROM ubuntu:24.04 AS runtime

EXPOSE 27910/udp

# Remove default ubuntu user
RUN userdel -r ubuntu

# Add the user UID:1000, GID:1000, home at /yquake2
RUN groupadd -r yquake2 -g 1000 && useradd -u 1000 -r -g yquake2 -m -d /yquake2 -s /sbin/nologin -c "yquake2 user" yquake2 && \
    chmod 755 /yquake2

WORKDIR /yquake2
COPY --from=build /yquake2/release ./

# Specify the user to execute all commands below
USER yquake2

RUN mkdir .yq2

VOLUME ["/yquake2/.yq2"]
ENTRYPOINT ["./q2ded"]
CMD ["+exec", "server.cfg"]

