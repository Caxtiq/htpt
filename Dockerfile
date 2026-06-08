FROM gcc:latest

WORKDIR /app
COPY c_src/node.c .

# Create /data directory for WAL
RUN mkdir -p /data

# Compile the C code with pthread for multi-threading
RUN gcc -o kv_node node.c -lpthread -O2

CMD ["./kv_node"]
