//
//  RPCTest.cpp
//  MNNTests
//

#include <atomic>
#include <thread>
#include <chrono>
#include <MNN/Interpreter.hpp>
#include <MNN/RPC.h>
#include "MNNTestSuite.h"
#include "backend/rpc/RPCClient.hpp"
#include "backend/rpc/RPCProtocol.hpp"
#include "backend/rpc/RPCServer.hpp"
#include "core/RuntimeFactory.hpp"

using namespace MNN;

class RPCProtocolHeaderTest : public MNNTestCase {
public:
    bool run(int precision) override {
        (void)precision;
#ifndef MNN_RPC_ENABLED
        return true;
#endif
        MNN::RPC::PacketHeader header;
        header.command = MNN_RPC_CMD_PING;
        header.flags = 7;
        header.requestId = 42;
        header.payloadSize = 128;
        auto encoded = MNN::RPC::encodeHeader(header);
        MNN::RPC::PacketHeader decoded;
        if (!MNN::RPC::decodeHeader(encoded.data(), encoded.size(), decoded)) {
            return false;
        }
        return decoded.command == header.command && decoded.flags == header.flags && decoded.requestId == header.requestId && decoded.payloadSize == header.payloadSize;
    }
};
MNNTestSuiteRegister(RPCProtocolHeaderTest, "rpc/protocol/header");

class RPCClientServerHandshakeTest : public MNNTestCase {
public:
    bool run(int precision) override {
        (void)precision;
#ifndef MNN_RPC_ENABLED
        return true;
#endif
#ifndef _WIN32
        MNN::RPC::ServerConfig serverConfig;
        serverConfig.host = "127.0.0.1";
        serverConfig.port = 28766;
        serverConfig.backendType = MNN_FORWARD_CPU;
        MNN::RPC::Server server(serverConfig);
        if (!server.start()) {
            return false;
        }
        std::atomic<bool> running{true};
        std::thread worker([&]() {
            while (running.load()) {
                server.serveOnce();
            }
            server.stop();
        });
        MNNRPCNodeConfig node;
        node.host = "127.0.0.1";
        node.port = 28766;
        node.backendType = MNN_FORWARD_CPU;
        node.deviceId = 0;
        node.weight = 1.0f;
        node.cachePath = nullptr;
        MNNRPCClientConfig cfg;
        cfg.structSize = sizeof(MNNRPCClientConfig);
        cfg.configVersion = MNN_RPC_CONFIG_VERSION;
        cfg.protocolVersion = MNN_RPC_PROTOCOL_VERSION;
        cfg.flags = MNN_RPC_REMOTE_F_ENABLE_GRAPH_CACHE;
        cfg.connectTimeoutMs = 1000;
        cfg.ioTimeoutMs = 1000;
        cfg.maxPacketSize = 1024 * 1024;
        cfg.authToken = nullptr;
        cfg.nodeCount = 1;
        cfg.nodes = &node;
        cfg.segmentCount = 0;
        cfg.segments = nullptr;

        MNN::RPC::ClientConfig internal;
        std::string error;
        bool valid = MNN::RPC::ClientConfig::from(&cfg, internal, error);
        if (!valid) {
            running = false;
            worker.join();
            return false;
        }
        bool ok = false;
        {
            MNN::RPC::Client client(internal);
            ok = client.connect() && client.hello() && client.ping();
            std::vector<MNNRPCDeviceCapability> capabilities;
            ok = ok && client.getCapabilities(capabilities) && capabilities.size() == 1 && capabilities[0].backendType == MNN_FORWARD_CPU;
        }

        BackendConfig backendConfig;
        backendConfig.sharedContext = &cfg;
        Backend::Info info;
        info.type = MNN_FORWARD_USER_2;
        info.user = &backendConfig;
        std::shared_ptr<Runtime> runtime(RuntimeFactory::create(info));
        ok = ok && runtime != nullptr;
        if (runtime != nullptr) {
            std::shared_ptr<Backend> backend(runtime->onCreate(&backendConfig));
            ok = ok && backend != nullptr;
        }
        running = false;
        // Give accept loop a chance to observe the stop flag without spinning forever.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        worker.join();
        return ok;
#else
        return true;
#endif
    }
};
MNNTestSuiteRegister(RPCClientServerHandshakeTest, "rpc/client/handshake");

class RPCGraphCacheLifecycleTest : public MNNTestCase {
public:
    bool run(int precision) override {
        (void)precision;
#ifndef MNN_RPC_ENABLED
        return true;
#endif
#ifndef _WIN32
        MNN::RPC::ServerConfig serverConfig;
        serverConfig.host = "127.0.0.1";
        serverConfig.port = 28767;
        serverConfig.backendType = MNN_FORWARD_CPU;
        MNN::RPC::Server server(serverConfig);
        if (!server.start()) {
            return false;
        }
        std::atomic<bool> running{true};
        std::thread worker([&]() {
            while (running.load()) {
                server.serveOnce();
            }
            server.stop();
        });

        MNNRPCNodeConfig node;
        node.host = "127.0.0.1";
        node.port = 28767;
        node.backendType = MNN_FORWARD_CPU;
        node.deviceId = 0;
        node.weight = 1.0f;
        node.cachePath = nullptr;
        MNNRPCClientConfig cfg;
        cfg.structSize = sizeof(MNNRPCClientConfig);
        cfg.configVersion = MNN_RPC_CONFIG_VERSION;
        cfg.protocolVersion = MNN_RPC_PROTOCOL_VERSION;
        cfg.flags = MNN_RPC_REMOTE_F_ENABLE_GRAPH_CACHE | MNN_RPC_REMOTE_F_ENABLE_WEIGHT_CACHE;
        cfg.connectTimeoutMs = 1000;
        cfg.ioTimeoutMs = 1000;
        cfg.maxPacketSize = 1024 * 1024;
        cfg.authToken = nullptr;
        cfg.nodeCount = 1;
        cfg.nodes = &node;
        cfg.segmentCount = 0;
        cfg.segments = nullptr;

        MNN::RPC::ClientConfig internal;
        std::string error;
        bool ok = MNN::RPC::ClientConfig::from(&cfg, internal, error);
        {
            MNN::RPC::Client client(internal);
            ok = ok && client.connect() && client.hello();
            MNN::RPC::GraphDescriptor graph;
            graph.graphUid = 0x1001;
            graph.segmentUid = 0x2002;
            graph.shapeSignature = 0x3003;
            graph.opCount = 4;
            graph.tensorCount = 6;
            bool cacheHit = false;
            ok = ok && client.registerGraph(graph, &cacheHit) && !cacheHit;
            ok = ok && client.registerGraph(graph, &cacheHit) && cacheHit;

            uint8_t weight[] = {1, 2, 3, 4};
            MNN::RPC::TensorDescriptor tensor;
            tensor.tensorUid = 0x4004;
            tensor.byteSize = sizeof(weight);
            tensor.dataType = 1;
            tensor.dimensions = 1;
            bool weightCacheHit = false;
            ok = ok && client.pushWeight(tensor, weight, sizeof(weight), &weightCacheHit) && !weightCacheHit;
            ok = ok && client.pushWeight(tensor, weight, sizeof(weight), &weightCacheHit) && weightCacheHit;

            MNN::RPC::RuntimeStats stats;
            ok = ok && client.getStats(stats);
            ok = ok && stats.graphCount == 1 && stats.weightCount == 1;
            ok = ok && stats.graphRegisterCount == 2 && stats.graphCacheHitCount == 1;
            ok = ok && stats.weightPushCount == 2 && stats.weightCacheHitCount == 1;
            ok = ok && client.freeGraph(graph.graphUid);
            ok = ok && client.getStats(stats) && stats.graphCount == 0;
        }

        running = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        worker.join();
        return ok;
#else
        return true;
#endif
    }
};
MNNTestSuiteRegister(RPCGraphCacheLifecycleTest, "rpc/cache/lifecycle");
