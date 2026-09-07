# Exile III: Ruined World - Self-Contained 32-bit Linux Game Container
FROM i386/ubuntu:trusty

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies and build tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    xserver-xephyr \
    x11-utils \
    x11-apps \
    pulseaudio-utils \
    libpulsedsp:i386 \
    xfonts-utils \
    xfonts-base \
    gcc \
    libc6-dev \
    libx11-dev \
    libfreetype6:i386 \
    make \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Set up working directory
WORKDIR /game

# Copy entire game directory (binaries, data files, fonts, and configs)
COPY . /game/

# Compile audio interceptor shim, index fonts, and ensure executables
RUN make -C /game \
    && cd /game/fonts && mkfontdir \
    && chmod +x /game/entrypoint.sh /game/exile3-binary /game/exile3ed-binary

# Default entrypoint
ENTRYPOINT ["/game/entrypoint.sh"]
CMD ["exile3"]
