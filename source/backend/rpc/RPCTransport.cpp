//
//  RPCTransport.cpp
//  MNN
//

#include "RPCTransport.hpp"

#include <errno.h>
#include <string.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace MNN {
namespace RPC {

Socket::Socket() : mFd(-1) {
}

Socket::Socket(int fd) : mFd(fd) {
}

Socket::~Socket() {
    close();
}

void Socket::setError(const std::string& error) {
    mLastError = error;
}

bool Socket::connect(const std::string& host, int port, int timeoutMs) {
#ifdef _WIN32
    setError("RPC transport is not implemented on Windows");
    return false;
#else
    close();
    struct addrinfo hints;
    ::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* result = nullptr;
    auto service = std::to_string(port);
    int res = ::getaddrinfo(host.c_str(), service.c_str(), &hints, &result);
    if (res != 0) {
        setError(::gai_strerror(res));
        return false;
    }
    for (auto rp = result; rp != nullptr; rp = rp->ai_next) {
        mFd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (mFd < 0) {
            continue;
        }
        int flag = 1;
        ::setsockopt(mFd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        setTimeout(timeoutMs);
        if (::connect(mFd, rp->ai_addr, rp->ai_addrlen) == 0) {
            ::freeaddrinfo(result);
            return true;
        }
        setError(::strerror(errno));
        close();
    }
    ::freeaddrinfo(result);
    return false;
#endif
}

bool Socket::bindAndListen(const std::string& host, int port, int backlog) {
#ifdef _WIN32
    setError("RPC transport is not implemented on Windows");
    return false;
#else
    close();
    mFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (mFd < 0) {
        setError(::strerror(errno));
        return false;
    }
    int reuse = 1;
    ::setsockopt(mFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    int flag = 1;
    ::setsockopt(mFd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    sockaddr_in addr;
    ::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (host.empty() || host == "0.0.0.0") {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        setError("Only IPv4 listen addresses are supported");
        close();
        return false;
    }
    if (::bind(mFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        setError(::strerror(errno));
        close();
        return false;
    }
    if (::listen(mFd, backlog) != 0) {
        setError(::strerror(errno));
        close();
        return false;
    }
    return true;
#endif
}

std::unique_ptr<Socket> Socket::accept(int timeoutMs) const {
#ifdef _WIN32
    (void)timeoutMs;
    return nullptr;
#else
    if (mFd < 0) {
        return nullptr;
    }
    if (timeoutMs >= 0) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(mFd, &rfds);
        timeval tv;
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        auto ready = ::select(mFd + 1, &rfds, nullptr, nullptr, &tv);
        if (ready <= 0) {
            return nullptr;
        }
    }
    auto fd = ::accept(mFd, nullptr, nullptr);
    if (fd < 0) {
        return nullptr;
    }
    int flag = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    return std::unique_ptr<Socket>(new Socket(fd));
#endif
}

bool Socket::sendAll(const void* data, size_t size) {
#ifdef _WIN32
    (void)data;
    (void)size;
    setError("RPC transport is not implemented on Windows");
    return false;
#else
    auto ptr = reinterpret_cast<const uint8_t*>(data);
    size_t offset = 0;
    while (offset < size) {
#ifdef MSG_NOSIGNAL
        auto sent = ::send(mFd, ptr + offset, size - offset, MSG_NOSIGNAL);
#else
        auto sent = ::send(mFd, ptr + offset, size - offset, 0);
#endif
        if (sent <= 0) {
            setError(::strerror(errno));
            return false;
        }
        offset += static_cast<size_t>(sent);
    }
    return true;
#endif
}

bool Socket::recvAll(void* data, size_t size) {
#ifdef _WIN32
    (void)data;
    (void)size;
    setError("RPC transport is not implemented on Windows");
    return false;
#else
    auto ptr = reinterpret_cast<uint8_t*>(data);
    size_t offset = 0;
    while (offset < size) {
        auto recved = ::recv(mFd, ptr + offset, size - offset, MSG_WAITALL);
        if (recved <= 0) {
            setError(::strerror(errno));
            return false;
        }
        offset += static_cast<size_t>(recved);
    }
    return true;
#endif
}

void Socket::close() {
#ifndef _WIN32
    if (mFd >= 0) {
        ::shutdown(mFd, SHUT_RDWR);
        ::close(mFd);
        mFd = -1;
    }
#else
    mFd = -1;
#endif
}

bool Socket::valid() const {
    return mFd >= 0;
}

int Socket::fd() const {
    return mFd;
}

std::string Socket::lastError() const {
    return mLastError;
}

void Socket::setTimeout(int timeoutMs) {
#ifndef _WIN32
    if (mFd < 0 || timeoutMs < 0) {
        return;
    }
    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    ::setsockopt(mFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(mFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

} // namespace RPC
} // namespace MNN
