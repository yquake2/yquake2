FROM ubuntu:18.04 AS build

WORKDIR /yquake2
RUN apt-get update
RUN apt-get install libcurl4-openssl-dev build-essential -y
COPY . .

RUN make server game

FROM ubuntu:18.04 AS runtime

ENV server_cfg="server.cfg"

EXPOSE 27910

# Add the user UID:1000, GID:1000, home at /yquake2
RUN groupadd -r yquake2 -g 1000 && useradd -u 1000 -r -g yquake2 -m -d /yquake2 -s /sbin/nologin -c "yquake2 user" yquake2 && \
    chmod 755 /yquake2

WORKDIR /yquake2
COPY --from=build /yquake2/release ./

# Specify the user to execute all commands below
USER yquake2

RUN mkdir .yq2
VOLUME ["/yquake2/.yq2"]
ENTRYPOINT ./q2ded +exec ${server_cfg}
