FROM amazonlinux:2

WORKDIR /app
COPY rust_tonic_mt_grpc_hnswlib_bench /app
COPY proto /app/proto

# Install rust 
RUN curl https://sh.rustup.rs -sSf | sh -s -- --default-toolchain nightly -y

RUN yum install -y cmake protobuf-compiler-grpc clang cargo wget unzip

# protoc for protobuf complilation
RUN wget https://github.com/protocolbuffers/protobuf/releases/download/v3.8.0/protoc-3.8.0-linux-x86_64.zip
RUN unzip protoc-3.8.0-linux-x86_64.zip -d /usr/local && rm protoc-3.8.0-linux-x86_64.zip

RUN cargo build --release --locked

ENTRYPOINT ["/app/target/release/helloworld-server"]
