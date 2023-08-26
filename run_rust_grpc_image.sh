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
RESULTS_DIR=results

mkdir -p "${RESULTS_DIR}"

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

echo "Benchmarking now... "
export GRPC_BENCHMARK_DURATION=${GRPC_BENCHMARK_DURATION:-"20s"}
export GRPC_CLIENT_CONNECTIONS=${GRPC_CLIENT_CONNECTIONS:-"50"}
export GRPC_CLIENT_CONCURRENCY=${GRPC_CLIENT_CONCURRENCY:-"1000"}
export GRPC_CLIENT_QPS=${GRPC_CLIENT_QPS:-"3"}
export GRPC_CLIENT_QPS=$(( GRPC_CLIENT_QPS / GRPC_CLIENT_CONCURRENCY ))
export GRPC_CLIENT_CPUS=${GRPC_CLIENT_CPUS:-"1"}
export GRPC_REQUEST_SCENARIO=${GRPC_REQUEST_SCENARIO:-"complex_proto"}
export GRPC_IMAGE_NAME="${GRPC_IMAGE_NAME:-grpc_bench}"
export GRPC_GHZ_TAG="${GRPC_GHZ_TAG:-0.114.0}"


# Start the gRPC Client
docker run --name ghz --rm --network=host -v "${PWD}/proto:/proto:ro" \
	-v "${PWD}/payload:/payload:ro" \
	--cpus $GRPC_CLIENT_CPUS \
    ghcr.io/bojand/ghz:"${GRPC_GHZ_TAG}" \
	--proto=/proto/helloworld/helloworld.proto \
	--call=helloworld.Greeter.SayHello \
        --disable-template-functions \
        --disable-template-data \
        --insecure \
        --concurrency="${GRPC_CLIENT_CONCURRENCY}" \
        --connections="${GRPC_CLIENT_CONNECTIONS}" \
        --rps="${GRPC_CLIENT_QPS}" \
        --duration "${GRPC_BENCHMARK_DURATION}" \
        --data-file /payload/payload \
		127.0.0.1:50051 >"${RESULTS_DIR}/${NAME}".report

# Show quick summary (reqs/sec)
cat << EOF
	done.
	Results:
	$(cat "${RESULTS_DIR}/${NAME}".report | grep "Requests/sec" | sed -E 's/^ +/    /')

EOF
