#include <err.h>

#include <event2/event.h>

#include <ight/common/pointer.hpp>

#include <ight/net/utp_transport.hpp>

using namespace ight::common::pointer;
using namespace ight::net::utp;

int
main()
{
    std::string buff;
    event_base *evbase;

    if ((evbase = event_base_new()) == nullptr) {
        throw std::runtime_error("event_base_new() failed");
    }

    /*
     * Server
     */

    auto server = std::make_shared<Context>("9876", evbase, 500000);
    server->on_accept([](utp_callback_arguments *args) {
        warnx("server: connection from %p", args->address);
    });
    server->on_read([&buff](utp_callback_arguments *args) {
        warnx("server: got data: %ld", args->len);
        buff += std::string((char *) args->buf, args->len);
        if (buff.length() > 1048576) {
            // TODO: pause reading in this case
        }
        while (buff.length() > 0) {
            auto result = utp_write(args->socket,
                                    (void *) buff.c_str(), // XXX
                                    buff.length());
            if (result < 0) {
                warnx("server: write error");
                break; // TODO: understand the implications of this
            } else if (result == 0) {
                break;
            }
            buff.erase(0, result);
        }
    });

    /*
     * Client
     */

    auto conn = std::make_shared<UTPTransport>("54321", evbase);

    conn->on_connect([&conn](void) { /* XXX: is it correct the capture? */
        for (int i = 0; i < 350; i++) {
            conn->send("BBBBBBBBBB");
        }
    });
    conn->on_flush([](void) {
    });
    conn->on_data([&conn](SharedPointer<Buffer> data) {
        warnx("client: got data: %ld", data->length());
        conn->print_stats();
        conn->close();
    });

    conn->connect("127.0.0.1", "9876");
    
    (void) event_base_dispatch(evbase);

    event_base_free(evbase);

    return (0);
}
