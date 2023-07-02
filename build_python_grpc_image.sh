export GRPC_REQUEST_SCENARIO=${GRPC_REQUEST_SCENARIO:-"complex_proto"}
export GRPC_IMAGE_NAME="${GRPC_IMAGE_NAME:-grpc_bench}"
benchmark=python_grpc_hnswlib_bench

builds=""
echo "==> Building Docker image for ${benchmark}..."
( (
	DOCKER_BUILDKIT=1 docker image build \
		--force-rm \
		--pull \
		--compress \
		--file "${benchmark}/Dockerfile" \
		--tag "$GRPC_IMAGE_NAME:${benchmark}-$GRPC_REQUEST_SCENARIO" \
		. >"${benchmark}.tmp" 2>&1 &&
		cat "${benchmark}.tmp" &&
		rm -f "${benchmark}.tmp" &&
		echo "==> Done building ${benchmark}"
) || (
	cat "${benchmark}.tmp"
	rm -f "${benchmark}.tmp"
	echo "==> Error building ${benchmark}"
	exit 1
) ) &
builds="${builds} ${!}"

echo "==> Waiting for the builds to finish..."
for job in ${builds}; do
	if ! wait "${job}"; then
		wait
		echo "Error building Docker image(s)"
		exit 1
	fi
done
echo "All done."
