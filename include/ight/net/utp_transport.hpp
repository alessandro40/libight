/*-
 * This file is part of Libight <https://libight.github.io/>.
 *
 * Libight is free software. See AUTHORS and LICENSE for more
 * information on the copying conditions.
 */

#ifndef IGHT_NET_UTP_TRANSPORT_HPP
#define IGHT_NET_UTP_TRANSPORT_HPP

#include <functional>
#include <string>
#include <iostream>

#include <event2/event.h>
#include <event2/util.h>

#include <ight/common/constraints.hpp>
#include <ight/common/pointer.hpp>
#include <ight/net/buffer.hpp>
#include <ight/net/transport.hpp>

#include "libutp/utp.h"

#define MTU 1200 /* FIXME: it should be taken from libutp sock stats */

struct event_base;
struct event;
struct sockaddr;

namespace ight {
namespace net {
namespace utp {

using namespace ight::common::constraints;
using namespace ight::common::pointer;

using namespace ight::net::buffer;
using namespace ight::net::transport;

class Context : public NonCopyable, public NonMovable {

    evutil_socket_t fd = -1;
    utp_context *utp_ctx = nullptr;
    event *ev_read = nullptr;
    event *ev_write = nullptr;
    event *ev_timeo = nullptr;	
    uint16_t port;

    event_base *ev_base = nullptr;  // Not owned by us

    std::function<void(utp_callback_arguments *)> dispatch_accept;
    std::function<void(utp_callback_arguments *)> dispatch_connect;
    std::function<void(utp_callback_arguments *)> dispatch_error;
    std::function<void(utp_callback_arguments *)> dispatch_read;
    std::function<void(utp_callback_arguments *)> dispatch_state_change;

    static void libevent_on_read(evutil_socket_t, short, void *);
    static void libevent_on_timeout(evutil_socket_t, short, void *);
    static void libevent_on_write(evutil_socket_t, short, void *);

    static uint64 libutp_sendto(utp_callback_arguments *);
    static uint64 libutp_on_accept(utp_callback_arguments *);
    static uint64 libutp_on_connect(utp_callback_arguments *);
    static uint64 libutp_on_error(utp_callback_arguments *);
    static uint64 libutp_on_read(utp_callback_arguments *);
    static uint64 libutp_on_state_change(utp_callback_arguments *);
    static uint64 libutp_log(utp_callback_arguments *);

    void cleanup();

    static int parse_port_(std::string port) {
        std::size_t pos;
        auto portnum = std::stoi(port, &pos);
        if (pos < port.length() || portnum < 0 || portnum > 65535) {
            throw std::runtime_error("Invalid port number");
        }
        return portnum;
    }

  public:

    ~Context() {
        cleanup();
    }

    Context(uint16_t, event_base *, int);
    Context(std::string port, event_base *evbase, int timeout)
            : Context(parse_port_(port), evbase, timeout) {}

    utp_socket *connect(sockaddr *);
    utp_socket *connect_ipv4(std::string address, std::string port);

    void on_accept(std::function<void(utp_callback_arguments *)> f) {
       dispatch_accept = f;
    }
    void on_connect(std::function<void(utp_callback_arguments *)> f) {
       dispatch_connect = f;
    }
    void on_error(std::function<void(utp_callback_arguments *)> f) {
       dispatch_error = f;
    }
    void on_read(std::function<void(utp_callback_arguments *)> f) {
       dispatch_read = f;
    }
    void on_state_change(std::function<void(utp_callback_arguments *)> f) {
       dispatch_state_change = f;
    }

    uint16_t get_port() {
        return port;
    }
};

class uConnection : public Transport {

    utp_socket *socket = nullptr;

    SharedPointer<Buffer> buf;
    SharedPointer<utp::Context> context;

    bool is_writable = false;

    std::function<void(void)> on_connect_fn = [](void) {
        /* nothing */
    };

    std::function<void(SharedPointer<Buffer>)> on_data_fn = [](
            SharedPointer<Buffer>) {
        /* nothing */
    };

    std::function<void(void)> on_flush_fn = [](void) {
        /* nothing */
    };

    void cleanup();

    void flush();

  public:
    virtual void emit_connect() {
        on_connect_fn();
    }

    virtual void emit_data(SharedPointer<Buffer> buf) {
        on_data_fn(buf);
    }

    virtual void emit_flush() {
        on_flush_fn();
    }

    virtual void emit_error(Error) {}

    virtual void on_connect(std::function<void()> fn) {
        on_connect_fn = fn;
    }

    virtual void on_ssl(std::function<void()>) {}

    virtual void on_data(std::function<void(SharedPointer<Buffer>)> fn) {
        on_data_fn = fn;
    }

    virtual void on_flush(std::function<void()> fn) {
        on_flush_fn = fn;
    }

    virtual void on_error(std::function<void(Error)>) {}

    virtual void set_timeout(double) {}

    virtual void clear_timeout() {}

    virtual void send(const void*, size_t);

    virtual void send(std::string);

    virtual void send(SharedPointer<Buffer>);

    virtual void send(Buffer&);

    virtual void close() {}

    virtual std::string socks5_address() {
        return ""; /* no proxy */
    }

    virtual std::string socks5_port() {
        return ""; /* no proxy */
    }

    uConnection(std::string local_port, event_base *);

    void connect(std::string address, std::string port);

    void print_stats();
};

}}} // namespaces

#endif /* IGHT_NET_UTP_TRANSPORT_HPP */
