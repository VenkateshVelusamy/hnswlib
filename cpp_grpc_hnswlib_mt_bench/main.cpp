/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <memory>
#include <iostream>
#include <string>
#include <thread>
#include "hnswlib/hnswlib.h"

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>

#ifdef BAZEL_BUILD
#include "examples/protos/helloworld.grpc.pb.h"
#else
#include "helloworld.grpc.pb.h"
#endif

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using helloworld::HelloRequest;
using helloworld::HelloReply;
using helloworld::Greeter;

template<class Function>
inline void ParallelFor(size_t start, size_t end, size_t numThreads, Function fn) {
    if (numThreads <= 0) {
        numThreads = std::thread::hardware_concurrency();
    }

    if (numThreads == 1) {
        for (size_t id = start; id < end; id++) {
            fn(id, 0);
        }
    } else {
        std::vector<std::thread> threads;
        std::atomic<size_t> current(start);

        // keep track of exceptions in threads
        // https://stackoverflow.com/a/32428427/1713196
        std::exception_ptr lastException = nullptr;
        std::mutex lastExceptMutex;

        for (size_t threadId = 0; threadId < numThreads; ++threadId) {
            threads.push_back(std::thread([&, threadId] {
                while (true) {
                    size_t id = current.fetch_add(1);

                    if (id >= end) {
                        break;
                    }

                    try {
                        fn(id, threadId);
                    } catch (...) {
                        std::unique_lock<std::mutex> lastExcepLock(lastExceptMutex);
                        lastException = std::current_exception();
                        /*
                         * This will work even when current is the largest value that
                         * size_t can fit, because fetch_add returns the previous value
                         * before the increment (what will result in overflow
                         * and produce 0 instead of current + 1).
                         */
                        current = end;
                        break;
                    }
                }
            }));
        }
        for (auto &thread : threads) {
            thread.join();
        }
        if (lastException) {
            std::rethrow_exception(lastException);
        }
    }
}


class ServerImpl final {
 public:
  ~ServerImpl() {
    server_->Shutdown();
    // Always shutdown the completion queue after the server.
    for (auto& cq : cq_)
      cq->Shutdown();
  }



  // There is no shutdown handling in this code.
  void Run() {
    std::string server_address("0.0.0.0:50051");

    int dim = 128;               // Dimension of the elements
    int max_elements = 10000;   // Maximum number of elements, should be known beforehand
    int M = 100;                 // Tightly connected with internal dimensionality of the data
                                // strongly affects the memory consumption
    int ef_construction = 1024;  // Controls index search speed/build speed tradeoff
    int ef_search = 10000;
    int num_threads = 10;

    std::cout << "Initializing HNSW index" << std::endl;
    // Initing index
    hnswlib::L2Space space(dim);
    alg_hnsw_ = new hnswlib::HierarchicalNSW<float>(&space, max_elements, M, ef_construction);

    std::cout << "Adding points to HNSW index" << std::endl;
    // Generate random data
    std::mt19937 rng;
    rng.seed(47);
    std::uniform_real_distribution<> distrib_real(0.0, 1.0);
    data_ = new float[dim * max_elements];
    for (int i = 0; i < dim * max_elements; i++) {
        data_[i] = distrib_real(rng);
        
    }

    ParallelFor(0, max_elements, num_threads, [&](size_t row, size_t threadId) {
        alg_hnsw_->addPoint((void*)(data_ + dim * row), row);
    });

    alg_hnsw_->setEf(ef_search);
    std::cout << "Index built" << std::endl;

    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // Register "service_" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *asynchronous* service.
    builder.RegisterService(&service_);
    // Get hold of the completion queue used for the asynchronous communication
    // with the gRPC runtime.
    auto parallelism = std::atoi(std::getenv("GRPC_SERVER_CPUS"));
    for (int i = 0; i < parallelism; i++) {
      cq_.emplace_back(builder.AddCompletionQueue());
    }
    // Finally assemble the server.
    server_ = builder.BuildAndStart();
    std::cout << "Server listening on " << server_address << std::endl;

    // Proceed to the server's main loop.
    for (int i = 0; i < parallelism; i++) {
      server_threads_.emplace_back(std::jthread([this, i] { this->HandleRpcs(i); }));
    }

    std::this_thread::sleep_until(std::chrono::time_point<std::chrono::system_clock>::max());
  }

 private:
  // Class encompasing the state and logic needed to serve a request.
  class CallData {
   public:
    // Take in the "service" instance (in this case representing an asynchronous
    // server) and the completion queue "cq" used for asynchronous communication
    // with the gRPC runtime.
    CallData(Greeter::AsyncService* service, ServerCompletionQueue* cq, hnswlib::HierarchicalNSW<float>* alg_hnsw, float * data)
        : service_(service), cq_(cq), responder_(&ctx_), alg_hnsw_(alg_hnsw), data_(data), status_(CREATE) {
      // Invoke the serving logic right away.
      Proceed();
    }

    void Proceed() {
      if (status_ == CREATE) {
        // Make this instance progress to the PROCESS state.
        status_ = PROCESS;

        // As part of the initial CREATE state, we *request* that the system
        // start processing SayHello requests. In this request, "this" acts are
        // the tag uniquely identifying the request (so that different CallData
        // instances can serve different requests concurrently), in this case
        // the memory address of this CallData instance.
        service_->RequestSayHello(&ctx_, &request_, &responder_, cq_, cq_,
                                  this);

        // Searching for 150th float vectors with k value of 1
        std::priority_queue<std::pair<float, hnswlib::labeltype>> result = alg_hnsw_->searchKnn((void*)(data_ + 128 * 150), 1);
        hnswlib::labeltype label = result.top().second;

        std::cout << "Searched element: "<< label << std::endl;   
      } else if (status_ == PROCESS) {
        // Spawn a new CallData instance to serve new clients while we process
        // the one for this CallData. The instance will deallocate itself as
        // part of its FINISH state.
        new CallData(service_, cq_, alg_hnsw_, data_);

        // The actual processing.
        *reply_.mutable_response() = std::move(*request_.mutable_request());

        // And we are done! Let the gRPC runtime know we've finished, using the
        // memory address of this instance as the uniquely identifying tag for
        // the event.
        status_ = FINISH;
        responder_.Finish(reply_, Status::OK, this);
      } else {
        GPR_ASSERT(status_ == FINISH);
        // Once in the FINISH state, deallocate ourselves (CallData).
        delete this;
      }
    }

   private:
    // The means of communication with the gRPC runtime for an asynchronous
    // server.
    Greeter::AsyncService* service_;
    // The producer-consumer queue where for asynchronous server notifications.
    ServerCompletionQueue* cq_;
    // Context for the rpc, allowing to tweak aspects of it such as the use
    // of compression, authentication, as well as to send metadata back to the
    // client.
    ServerContext ctx_;

    // What we get from the client.
    HelloRequest request_;
    // What we send back to the client.
    HelloReply reply_;

    // The means to get back to the client.
    ServerAsyncResponseWriter<HelloReply> responder_;


    hnswlib::HierarchicalNSW<float>* alg_hnsw_;
    float* data_;

    // Let's implement a tiny state machine with the following states.
    enum CallStatus { CREATE, PROCESS, FINISH };
    CallStatus status_;  // The current serving state.
  };

  // This can be run in multiple threads if needed.
  void HandleRpcs(int i) {
    // Spawn a new CallData instance to serve new clients.
    new CallData(&service_, cq_[i].get(), alg_hnsw_, data_);
    void* tag;  // uniquely identifies a request.
    bool ok;
    while (true) {
      // Block waiting to read the next event from the completion queue. The
      // event is uniquely identified by its tag, which in this case is the
      // memory address of a CallData instance.
      // The return value of Next should always be checked. This return value
      // tells us whether there is any kind of event or cq_ is shutting down.
      GPR_ASSERT(cq_[i]->Next(&tag, &ok));
      // GPR_ASSERT(ok);
      static_cast<CallData*>(tag)->Proceed();
    }
  }

  std::vector<std::unique_ptr<ServerCompletionQueue>> cq_;
  Greeter::AsyncService service_;
  std::unique_ptr<Server> server_;
  std::vector<std::jthread> server_threads_;
  hnswlib::HierarchicalNSW<float>* alg_hnsw_;
  float* data_;
};

int main(int argc, char** argv) {
  ServerImpl server;
  server.Run();

  return 0;
}
