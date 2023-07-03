wait_on_tcp50051() {
  for ((i=1;i<=10*30;i++)); 
      do
	 echo "waiting for server"
	 nc -z localhost 50051 && return 0
	 sleep .1
      done
  return 1
}


export GRPC_SERVER_CPUS=${GRPC_SERVER_CPUS:-"4"}
export GRPC_SERVER_RAM=${GRPC_SERVER_RAM:-"1024m"}
export GRPC_REQUEST_SCENARIO=${GRPC_REQUEST_SCENARIO:-"complex_proto"}
export GRPC_IMAGE_NAME="${GRPC_IMAGE_NAME:-grpc_bench}"
NAME=rust_tonic_mt_grpc_hnswlib_bench

# Start the gRPC Server container
docker run \
	--name "${NAME}" \
	--rm \
	--cpus "${GRPC_SERVER_CPUS}" \
	--memory "${GRPC_SERVER_RAM}" \
	--security-opt seccomp=unconfined \
	-e GRPC_SERVER_CPUS \
	-e GRPC_SERVER_RAM \
	-p 50051:50051 \
	--detach \
	--tty \
	"$GRPC_IMAGE_NAME:${NAME}-$GRPC_REQUEST_SCENARIO" >/dev/null

printf 'Waiting for server to come up... '
if ! wait_on_tcp50051; then
   echo 'server unresponsive!'
   exit 1
fi

echo 'ready.'
