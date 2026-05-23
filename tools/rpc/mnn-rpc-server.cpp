//
//  mnn-rpc-server.cpp
//  MNN
//

#include <atomic>
#include <cstdlib>
#include <csignal>
#include <iostream>
#include <thread>
#include "backend/rpc/RPCServer.hpp"

static std::atomic<bool> gKeepRunning(true);

static void signalHandler(int) {
    gKeepRunning = false;
}

int main(int argc, char** argv) {
    MNN::RPC::ServerConfig config;
    config.host = argc > 1 ? argv[1] : "0.0.0.0";
    config.port = argc > 2 ? ::atoi(argv[2]) : 28765;
    config.backendType = argc > 3 ? static_cast<MNNForwardType>(::atoi(argv[3])) : MNN_FORWARD_CPU;
    config.deviceId = argc > 4 ? ::atoi(argv[4]) : 0;
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    MNN::RPC::Server server(config);
    if (!server.start()) {
        std::cerr << "failed to start RPC server: " << server.lastError() << std::endl;
        return 1;
    }
    std::cout << "mnn-rpc-server listening on " << config.host << ':' << config.port << std::endl;
    while (gKeepRunning.load()) {
        if (!server.serveOnce() && !server.valid()) {
            std::cerr << "rpc server stopped: " << server.lastError() << std::endl;
            return 2;
        }
    }
    server.stop();
    return 0;
}
