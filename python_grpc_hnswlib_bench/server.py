# Copyright 2015 gRPC authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""The Python implementation of the GRPC helloworld.Greeter server."""

from concurrent import futures

import grpc
import hnswlib
import numpy as np

import helloworld_pb2
import helloworld_pb2_grpc


class Greeter(helloworld_pb2_grpc.GreeterServicer):
    def __init__(self, index, data):
        self.index = index
        self.data = data

    def SayHello(self, request, context):
        labels, distances = self.index.knn_query(self.data[10], k = 1)
        print("Searched item: ", labels)
        return helloworld_pb2.HelloReply(response=request.request)


def serve():
    dim = 128
    num_elements = 10000

    # Generating sample data
    data = np.float32(np.random.random((num_elements, dim)))
    ids = np.arange(num_elements)

    # Declaring index
    p = hnswlib.Index(space = 'l2', dim = dim) # possible options are l2, cosine or ip

    # Initializing index - the maximum number of elements should be known beforehand
    p.init_index(max_elements = num_elements, ef_construction = 1024, M = 100)

    # Element insertion (can be called several times):
    p.add_items(data, ids)

    # Controlling the recall by setting ef:
    p.set_ef(10000) # ef should always be > k

    server = grpc.server(futures.ThreadPoolExecutor(max_workers=4))
    helloworld_pb2_grpc.add_GreeterServicer_to_server(Greeter(p, data), server)
    server.add_insecure_port('[::]:50051')
    server.start()
    print("Server started listening on port 50051")
    server.wait_for_termination()


if __name__ == '__main__':
    serve()
