//
//  RPCTransport.hpp
//  MNN
//

#ifndef MNN_SOURCE_BACKEND_RPC_RPCTRANSPORT_HPP
#define MNN_SOURCE_BACKEND_RPC_RPCTRANSPORT_HPP

#include <memory>
#include <string>

namespace MNN {
namespace RPC {

class Socket {
public:
    Socket();
    explicit Socket(int fd);
    ~Socket();

    bool connect(const std::string& host, int port, int timeoutMs);
    bool bindAndListen(const std::string& host, int port, int backlog);
    std::unique_ptr<Socket> accept(int timeoutMs = -1) const;
    bool sendAll(const void* data, size_t size);
    bool recvAll(void* data, size_t size);
    void close();
    bool valid() const;
    int fd() const;
    std::string lastError() const;
    void setTimeout(int timeoutMs);

private:
    void setError(const std::string& error);
    int mFd;
    std::string mLastError;
};

} // namespace RPC
} // namespace MNN

#endif
