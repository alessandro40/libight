/*-
 * This file is part of Libight <https://libight.github.io/>.
 *
 * Libight is free software. See AUTHORS and LICENSE for more
 * information on the copying conditions.
 */


#include <functional>

#include <arpa/inet.h>
#include <errno.h>
#include <err.h>
#include <string.h>

#include <event2/event.h>

#include <libutp/utp.h>

#include <ight/net/utp_transport.hpp>

using namespace ight::net::utp;

void Context::libevent_on_read(int, short, void *opaque) {
    auto context = static_cast<Context*>(opaque);

    byte buffer[8192];
    ssize_t result;
    sockaddr_storage sa;
    socklen_t salen;

    memset(&sa, 0, sizeof (sa));
    salen = sizeof (sa);

    /*
     * TODO: research which is the proper way to implement the loop
     * below, that is, which is the best number of packets to read
     * at any given time from the socket.
     */
    for (int i = 0; i < 4; ++i) {

        if ((result = recvfrom(context->fd, (char *) buffer, sizeof (buffer),
                0, (sockaddr *) &sa, &salen)) > 0) {
            warnx("Context: %ld bytes read (UDP)", result);

            if (utp_process_udp(context->utp_ctx, buffer, (size_t) result,
                    (const sockaddr *) &sa, salen) == 0) {
                warnx("Context: UDP payload not recognized as UTP packet");
            }

            utp_issue_deferred_acks(context->utp_ctx);

        } else if (errno == EAGAIN) {
            break;

        } else {
            warn("Context: recvfrom() failed");
            break;
        }
    }
}

void Context::libevent_on_write(int, short, void *) {
    //auto context = static_cast<Context*>(opaque);
    throw std::runtime_error("Should not happen");
}

void Context::libevent_on_timeout(int, short, void *opaque) {
    auto context = static_cast<Context*>(opaque);
    utp_check_timeouts(context->utp_ctx);
}

uint64 Context::libutp_sendto(utp_callback_arguments *args) {
    auto ctx = static_cast<Context*>(utp_context_get_userdata(args->context));
    auto result = sendto(ctx->fd, args->buf, args->len, 0,
            args->address, args->address_len);

    // TODO: do we care about datagram truncation?
    // Libutp is interested in fragmentation only
    // in the MTU discovering process (that we do
    // not implement).
    if (result < 0)
        warnx("Context: sendto() failed");
    else
        warnx("Context: %ld bytes sent (UDP)", result);

    return (0);  // libutp is not interested in the return value
}

uint64 Context::libutp_on_accept(utp_callback_arguments *args) {
    auto ctx = static_cast<Context*>(utp_context_get_userdata(args->context));
    ctx->dispatch_accept(args);
    return 0;
}

uint64 Context::libutp_on_connect(utp_callback_arguments *args) {
    auto ctx = static_cast<Context*>(utp_context_get_userdata(args->context));
    ctx->dispatch_connect(args);
    return 0;
}

uint64 Context::libutp_on_error(utp_callback_arguments *args) {
    auto ctx = static_cast<Context*>(utp_context_get_userdata(args->context));
    ctx->dispatch_error(args);
    return 0;
}

uint64 Context::libutp_on_read(utp_callback_arguments *args) {
    auto ctx = static_cast<Context*>(utp_context_get_userdata(args->context));
    ctx->dispatch_read(args);
    return 0;
}

uint64 Context::libutp_on_state_change(utp_callback_arguments *args) {
    auto ctx = static_cast<Context*>(utp_context_get_userdata(args->context));
    ctx->dispatch_state_change(args);
    return 0;
}

uint64 Context::libutp_log(utp_callback_arguments *args) {
    // TODO: implement
    (void) args;
    return 0;
}

void Context::cleanup() {
    if (fd != -1) {
        (void) close(fd);
        fd = -1;
    }
    if (utp_ctx != nullptr) {
        utp_destroy(utp_ctx);
        utp_ctx = nullptr;
    }
    if (ev_read != nullptr) {
        event_free(ev_read);
        ev_read = nullptr;
    }
    if (ev_write != nullptr) {
        event_free(ev_write);
        ev_write = nullptr;
    }
    if (ev_timeo != nullptr) {
        event_free(ev_timeo);
        ev_timeo = nullptr;
    }
}

Context::Context(uint16_t port_, event_base *evbase_,
        int timeo_interval) : port(port_), ev_base(evbase_) {

    sockaddr_storage salocal;
    sockaddr_in *sin;
    timeval tv;

    memset(&salocal, 0, sizeof (salocal));
    sin = (sockaddr_in *) &salocal;
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);

    if ((utp_ctx = utp_init(2)) == nullptr) {  // Hardcoded
        cleanup();
        throw std::runtime_error("Context: utp_init() failed");
    }
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {  // XXX
        cleanup();
        throw std::runtime_error("Context: socket() failed");
    }
    if (evutil_make_socket_nonblocking(fd) < 0) {
        cleanup();
        throw std::runtime_error("Context: evutil_make_socket_nonblocking() failed");
    }
    if (bind(fd, (sockaddr *) sin, sizeof (*sin)) < 0) {
        cleanup();
        throw std::runtime_error("Context: bind() failed");
    }

    utp_context_set_userdata(utp_ctx, this);

    utp_set_callback(utp_ctx, UTP_SENDTO, libutp_sendto);
    utp_set_callback(utp_ctx, UTP_ON_ACCEPT, libutp_on_accept);
    utp_set_callback(utp_ctx, UTP_ON_CONNECT, libutp_on_connect);
    utp_set_callback(utp_ctx, UTP_ON_ERROR, libutp_on_error);
    utp_set_callback(utp_ctx, UTP_ON_READ, libutp_on_read);
    utp_set_callback(utp_ctx, UTP_ON_STATE_CHANGE, libutp_on_state_change);
    utp_set_callback(utp_ctx, UTP_LOG, libutp_log);

    if ((ev_read = event_new(ev_base, fd, EV_READ | EV_PERSIST,
            libevent_on_read, this)) == nullptr) {
        cleanup();
        throw std::runtime_error("Context: event_new() failed");
    }
    if (event_add(ev_read, nullptr) < 0) {
        cleanup();
        throw std::runtime_error("Context: event_add() failed");
    }

    /*
     * Disabled code until we figure out the proper way to do this:
     * the proper way to implement the on_flush() callback is
     * to look at the libutp side only, since a UDP socket
     * is always writable.
     */
#if 0
    if ((ev_write = event_new(ev_base, fd, EV_WRITE | EV_PERSIST,
            libevent_on_write, this)) == nullptr) {
        cleanup();
        throw std::runtime_error("Context: event_new() failed");
    }
    if (event_add(ev_write, nullptr) < 0) {
        cleanup();
        throw std::runtime_error("Context: event_add() failed");
    }
#endif

    if ((ev_timeo = event_new(ev_base, -1, EV_PERSIST,
            libevent_on_timeout, this)) == nullptr) {
        cleanup();
        throw std::runtime_error("Context: event_new() failed");
    }
    memset(&tv, 0, sizeof(tv));
    tv.tv_usec = timeo_interval;
    if (event_add(ev_timeo, &tv) < 0) {
        cleanup();
        throw std::runtime_error("Context: event_add() failed");
    }
} 

utp_socket *Context::connect(sockaddr *saddr) {
    utp_socket *sock;

    if ((sock = utp_create_socket(utp_ctx)) == nullptr) {
        throw std::runtime_error("utp: utp_create_socket() failed");
    }

    if (utp_connect(sock, saddr, sizeof (*saddr)) != 0) {
        utp_close(sock);
        throw std::runtime_error("UTPSocket: utp_connect() failed");
    }

    return sock;
}

utp_socket *Context::connect_ipv4(std::string address, std::string port) {
    sockaddr_storage storage;
    memset(&storage, 0, sizeof (storage));
    sockaddr_in *sin = (struct sockaddr_in *) &storage;
    sin->sin_family = AF_INET;
    sin->sin_port = htons(parse_port_(port));
    auto result = inet_pton(AF_INET, address.c_str(), &sin->sin_addr);
    if (result != 1) {
        throw std::runtime_error("inet_pton() failed");
    }
    return connect((sockaddr *) sin);
}

uConnection::uConnection(std::string port, event_base *evbase)
{
    // socket: nothing to do

    buf = std::make_shared<Buffer>();
    context = std::make_shared<Context>(port, evbase, 500000);
    context->on_state_change([this](utp_callback_arguments *args) {
        if (args->state == 2 || args->state == 1) {
            is_writable = true;
            this->flush(); 
        }
    });

    // is_writable: nothing to do
}

void
uConnection::cleanup()
{
    if (socket != nullptr) {
        utp_close(socket);
        free(socket);
    }

    // buf: nothing to do
    // context: nothing to do

    // is_writable: nothing to do
}

void
uConnection::connect(std::string address, std::string port)
{
    context->on_connect([this](utp_callback_arguments *args) {
        (void) args;
        is_writable = true;
        on_connect_fn();
    });
    context->on_read([this](utp_callback_arguments *args) {
        SharedPointer<Buffer> read_buf = std::make_shared<Buffer>();
        read_buf->write(args->buf, args->len);
        on_data_fn(read_buf);
    });

    socket = context->connect_ipv4(address, port);
}

void
uConnection::send(const void *base, size_t count)
{
    buf->write(base, count);
    if (is_writable) {
        flush();
    }
}

void
uConnection::send(std::string s)
{
    *buf << s; // copy
    if (is_writable) {
        flush();
    }
}

void
uConnection::send(SharedPointer<Buffer> data)
{
    *buf << *data;
    if (is_writable) {
        flush();
    }
}

void
uConnection::send(Buffer& sourcebuf)
{
    *buf << sourcebuf;
    if (is_writable) {
        flush();
    }
}

void
uConnection::flush()
{
    while (buf->length() > 0) {
        auto data = buf->peek(MTU); // copy
        auto res = utp_write(socket, (void *) data.c_str(), data.length()); // copy
    
        if (res < 0) {
            throw std::runtime_error("Error in utp_write()");
        }
        else if (res == 0) {
            is_writable = false;
            return;
        }
        buf->discard(res);
    }

    on_flush_fn();
}

void
uConnection::print_stats()
{
    utp_socket_stats *stats = utp_get_stats(socket);

    warnx("================== STATISTICS ==================");

    warnx("Total bytes received: %lld", stats->nbytes_recv);
    warnx("Total bytes transmit: %lld", stats->nbytes_recv);
    warnx("Retransmit counter: %d", stats->rexmit);
    warnx("Fast retransmit counter: %d", stats->fastrexmit);
    warnx("Transmit counter: %d", stats->nxmit);
    warnx("Receive counter: %d", stats->nrecv);
    warnx("Duplicate receive counter: %d", stats->nduprecv);
    warnx("Best guess at MTU: %d", stats->mtu_guess);
//    warnx("In-flight bytes: %d", socket->cur_window);
//    warnx("Congestion window size: %d", socket->max_window);
}
