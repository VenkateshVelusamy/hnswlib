FROM python:3.11

WORKDIR /app
COPY python_grpc_hnswlib_bench /app
COPY proto /app/proto

RUN python -m pip install grpcio grpcio-tools
RUN python -m grpc_tools.protoc -I/app/proto/helloworld --python_out=. --grpc_python_out=. helloworld.proto
RUN python -m pip install .

ENTRYPOINT [ "python", "/app/server.py" ]
