FROM rust:1.70

WORKDIR /app
COPY rust_tonic_mt_grpc_hnswlib_bench /app
COPY proto /app/proto

# gcc-multilib, libclang-dev needed for autocxx
RUN apt-get update && apt-get install -y cmake protobuf-compiler-grpc gcc-multilib libclang-dev
RUN rustup component add rustfmt
RUN cargo build --release --locked

ENTRYPOINT ["/app/target/release/helloworld-server"]
