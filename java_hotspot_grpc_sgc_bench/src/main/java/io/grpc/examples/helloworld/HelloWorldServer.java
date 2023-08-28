/*
 * Copyright 2015 The gRPC Authors
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
 */

package io.grpc.examples.helloworld;

import io.grpc.Server;
import io.grpc.ServerBuilder;
import io.grpc.stub.StreamObserver;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import java.util.logging.Logger;
import java.util.concurrent.Executors;

import com.stepstone.search.hnswlib.jna.Index;
import com.stepstone.search.hnswlib.jna.QueryTuple;
import com.stepstone.search.hnswlib.jna.SpaceName;

import java.io.File;
import java.time.Instant;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.ExecutorService;

/**
 * Server that manages startup/shutdown of a {@code Greeter} server.
 */
public class HelloWorldServer {

  

  private static final Logger logger = Logger.getLogger(HelloWorldServer.class.getName());
  private Server server;

  private void start(Index index, Map<Integer, float[]> vectorsMap) throws IOException {
    /* The port on which the server should run */
    var port = 50051;
    var serverBuilder = configureExecutor(ServerBuilder.forPort(port));
    server = serverBuilder.addService(new GreeterImpl(index, vectorsMap)).build().start();
    logger.info("Server started, listening on " + port);
    Runtime.getRuntime().addShutdownHook(new Thread(() -> {
      // Use stderr here since the logger may have been
      // reset by its JVM shutdown hook.
      System.err.println("*** shutting down gRPC server since JVM is shutting down");
      try {
        server.shutdown().awaitTermination(30, TimeUnit.SECONDS);
      } catch (InterruptedException e) {
        e.printStackTrace();
      }
      System.err.println("*** server shut down");
    }));
  }

  /**
   * Allow customization of the Executor with two environment variables:
   *
   * <p>
   * <ul>
   * <li>JVM_EXECUTOR_TYPE: direct, workStealing, single, fixed, cached</li>
   * <li>JVM_EXECUTOR_THREADS: integer value.</li>
   * </ul>
   * </p>
   *
   * The number of Executor Threads will default to the number of
   * availableProcessors(). Only the workStealing and fixed executors will use
   * this value.
   */
  private ServerBuilder<?> configureExecutor(ServerBuilder<?> sb) {
    var threads = System.getenv("JVM_EXECUTOR_THREADS");
    var i_threads = Runtime.getRuntime().availableProcessors();
    System.out.println("Available processors " + Runtime.getRuntime().availableProcessors());
    if (threads != null && !threads.isEmpty()) {
      i_threads = Integer.parseInt(threads);
    }

    var value = System.getenv().getOrDefault("JVM_EXECUTOR_TYPE", "fixed");
    switch (value) {
      case "direct" -> sb = sb.directExecutor();
      case "single" -> sb = sb.executor(Executors.newSingleThreadExecutor());
      case "fixed" -> sb = sb.executor(Executors.newFixedThreadPool(i_threads));
      case "workStealing" -> sb = sb.executor(Executors.newWorkStealingPool(i_threads));
      case "cached" -> sb = sb.executor(Executors.newCachedThreadPool());
    }

    return sb;
  }

  /**
   * Await termination on the main thread since the grpc library uses daemon
   * threads.
   */
  private void blockUntilShutdown() throws InterruptedException {
    if (server != null) {
      server.awaitTermination();
    }
  }

  private static float[] getRandomFloatArray(int dimension){
    float[] array = new float[dimension];
    Random random = new Random();
    for (int i = 0; i < dimension; i++){
        array[i] = random.nextFloat();
    }
    return array;
  }

  /**
   * Main launches the server from the command line.
   */
  public static void main(String[] args) throws IOException, InterruptedException {
    var serverApp = new HelloWorldServer();
    int numberOfItems = 10_000;
    int numberOfThreads = Runtime.getRuntime().availableProcessors(); /* try numberOfThreads = 1 to see the difference ;D */

    System.out.println("Initializing Index");
    /* this step is just to have some content for indexing (if you have your vectors, you're good to go) */
    Map<Integer, float[]> vectorsMap = new HashMap<>(numberOfItems);
    for (int i = 0; i < numberOfItems; i++){
	vectorsMap.put(i , getRandomFloatArray(128));
    }

    Index index = new Index(SpaceName.L2, 128);
    index.initialize(numberOfItems, 100, 1024, 200); /* set maxNumberOfElements, m, efConstruction and randomSeed */
    index.setEf(10000);

    long startTime = Instant.now().getEpochSecond();
    ExecutorService executorService = Executors.newFixedThreadPool(numberOfThreads);
    for (Map.Entry<Integer, float[]> entry : vectorsMap.entrySet()) {
         executorService.submit( () -> index.addItem(entry.getValue(), entry.getKey()) );
    }

    executorService.shutdown();
    executorService.awaitTermination(10, TimeUnit.MINUTES);
    System.out.println("Index initialized");

    // Set JNA path
    File projectFolder = new File("app/lib");
    System.setProperty("jna.library.path", projectFolder.getAbsolutePath());

    serverApp.start(index, vectorsMap);
    System.out.println("Server initialized");
    serverApp.blockUntilShutdown();
  }

  static class GreeterImpl extends GreeterGrpc.GreeterImplBase {
   
    private Index index;
    private Map<Integer, float[]> vectorsMap;

    public GreeterImpl(Index index, Map<Integer, float[]> vectors) {
       this.index = index;
       this.vectorsMap = vectors;
    }

    @Override
    public void sayHello(HelloRequest req, StreamObserver<HelloReply> responseObserver) {
      // search for 150th element
      QueryTuple qt = index.knnQuery(vectorsMap.get(150), 1);
      System.out.println("Searched elements: " + Arrays.toString(qt.getIds()));

      final var reply = HelloReply.newBuilder().setResponse(req.getRequest()).build();
      responseObserver.onNext(reply);
      responseObserver.onCompleted();
    }
  }
}
