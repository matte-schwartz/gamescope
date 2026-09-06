#include <catch2/catch_test_macros.hpp>

#include "HostKeyboard.h"
#include <cstdlib>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>
#include <sys/mman.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>
#include <wayland-client.h>
#include <string>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>
#include "wlr_begin.hpp"
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>
#include "wlr_end.hpp"

namespace
{
    gamescope::XkbKeymap Keymap( const char *pLayout, const char *pOptions = "" )
    {
        xkb_context *pContext = xkb_context_new( XKB_CONTEXT_NO_ENVIRONMENT_NAMES );
        REQUIRE( pContext );
        xkb_rule_names rules = { .layout = pLayout, .options = pOptions };
        xkb_keymap *pKeymap = xkb_keymap_new_from_names( pContext, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS );
        REQUIRE( pKeymap );
        xkb_context_unref( pContext );
        return gamescope::XkbKeymap{ pKeymap };
    }

    struct Keyboard
    {
        wlr_keyboard keyboard{};
        gamescope::CHostKeyboard host;

        explicit Keyboard( bool bOverride = false ) : host{ bOverride }
        {
            wlr_keyboard_init( &keyboard, nullptr, "test-host" );
            xkb_context *context = xkb_context_new( XKB_CONTEXT_NO_ENVIRONMENT_NAMES );
            xkb_rule_names rules = { .layout = "us" };
            xkb_keymap *keymap = xkb_keymap_new_from_names( context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS );
            REQUIRE( keymap );
            REQUIRE( wlr_keyboard_set_keymap( &keyboard, keymap ) );
            xkb_keymap_unref( keymap );
            xkb_context_unref( context );
        }
        ~Keyboard()
        {
            wlr_keyboard_finish( &keyboard );
        }
        void Map( const char *layout, const char *options = "" )
        {
            REQUIRE( host.SetKeymap( Keymap( layout, options ) ) );
        }
        void Attach() { REQUIRE( host.Attach( &keyboard ) ); }
        xkb_keysym_t Sym( uint32_t key ) { return xkb_state_key_get_one_sym( keyboard.xkb_state, key + 8 ); }
    };
}

TEST_CASE( "Host keymap and state received before startup retain ownership", "[host_keyboard]" )
{
    Keyboard k;
    k.Map( "us,de" );
    k.host.SetModifiers( 0, 0, 0, 1 );
    REQUIRE( k.Sym( KEY_Y ) == XKB_KEY_y );
    k.Attach();
    REQUIRE( k.Sym( KEY_Y ) == XKB_KEY_z );
    REQUIRE( k.keyboard.modifiers.group == 1 );
    REQUIRE( k.keyboard.group == nullptr );
}

TEST_CASE( "Host layout selection and complete map replacements both reach the keyboard", "[host_keyboard]" )
{
    Keyboard k;
    k.Attach();
    k.Map( "us,de" );
    k.host.SetModifiers( 0, 0, 0, 1 );
    REQUIRE( k.Sym( KEY_Y ) == XKB_KEY_z );
    k.host.SetModifiers( 0, 0, 0, 0 );
    REQUIRE( k.Sym( KEY_Y ) == XKB_KEY_y );
    k.Map( "fr" );
    REQUIRE( k.Sym( KEY_Q ) == XKB_KEY_a );
    REQUIRE( k.keyboard.group == nullptr );
}

TEST_CASE( "Host modifiers include AltGr and locks without replaying lock actions", "[host_keyboard]" )
{
    Keyboard k;
    k.Attach();
    k.Map( "de" );
    uint32_t altgr = 1u << xkb_keymap_mod_get_index( k.keyboard.keymap, "Mod5" );
    uint32_t caps = 1u << xkb_keymap_mod_get_index( k.keyboard.keymap, "Lock" );
    k.host.SetModifiers( altgr, 0, 0, 0 );
    REQUIRE( k.Sym( KEY_Q ) == XKB_KEY_at );
    k.host.SetModifiers( 0, 0, caps, 0 );
    k.host.Key( KEY_CAPSLOCK, true, 1 );
    k.host.Key( KEY_CAPSLOCK, false, 2 );
    REQUIRE( k.Sym( KEY_A ) == XKB_KEY_A );
    REQUIRE( k.keyboard.modifiers.locked == caps );
}

TEST_CASE( "An explicit override keeps its map and derives its own modifiers", "[host_keyboard]" )
{
    Keyboard k{ true };
    k.Attach();
    k.Map( "de" );
    k.host.SetModifiers( ~0u, ~0u, ~0u, 1 );
    REQUIRE( k.Sym( KEY_Y ) == XKB_KEY_y );
    REQUIRE( k.keyboard.modifiers.group == 0 );
    REQUIRE( k.keyboard.modifiers.locked == 0 );
    k.host.Key( KEY_LEFTSHIFT, true, 1 );
    REQUIRE( k.Sym( KEY_A ) == XKB_KEY_A );
    REQUIRE( k.keyboard.modifiers.depressed != 0 );
    k.host.Key( KEY_LEFTSHIFT, false, 2 );
    REQUIRE( k.Sym( KEY_A ) == XKB_KEY_a );
}

TEST_CASE( "Losing focus releases held keys while retaining the host layout and locks", "[host_keyboard]" )
{
    Keyboard k;
    k.Attach();
    k.Map( "us,de" );
    uint32_t shift = 1u << xkb_keymap_mod_get_index( k.keyboard.keymap, "Shift" );
    k.host.Key( KEY_LEFTSHIFT, true, 1 );
    k.host.SetModifiers( shift, 0, 0, 1 );
    REQUIRE( k.Sym( KEY_Y ) == XKB_KEY_Z );
    k.host.Key( KEY_LEFTSHIFT, false, 2 );
    k.host.Leave();
    REQUIRE( k.keyboard.num_keycodes == 0 );
    REQUIRE( k.keyboard.modifiers.depressed == 0 );
    REQUIRE( k.Sym( KEY_Y ) == XKB_KEY_z );
}

TEST_CASE( "A keyboard can appear late and return after capability removal", "[host_keyboard]" )
{
    Keyboard k;
    k.Attach();
    k.host.Key( KEY_LEFTSHIFT, true, 1 );
    REQUIRE( k.Sym( KEY_A ) == XKB_KEY_A );
    k.host.Key( KEY_LEFTSHIFT, false, 2 );
    k.Map( "us,de" );
    k.host.SetModifiers( 0, 0, 0, 1 );
    REQUIRE( k.Sym( KEY_Y ) == XKB_KEY_z );
    k.host.Reset();
    REQUIRE( k.keyboard.modifiers.group == 0 );
    k.Map( "fr" );
    REQUIRE( k.Sym( KEY_Q ) == XKB_KEY_a );
}


namespace
{
    // A real Wayland client and wlroots seat connected by a socket pair. No
    // renderer, GPU or running desktop is needed to observe the wire protocol.
    struct WireKeyboard
    {
        Keyboard k;
        wl_display *server = wl_display_create();
        wlr_seat *seat = nullptr;
        wl_display *client = nullptr;
        wl_client *serverClient = nullptr;
        wl_registry *registry = nullptr;
        wl_seat *clientSeat = nullptr;
        wl_keyboard *clientKeyboard = nullptr;
        wl_compositor *clientCompositor = nullptr;
        wl_surface *clientSurface = nullptr;
        xkb_context *context = xkb_context_new( XKB_CONTEXT_NO_ENVIRONMENT_NAMES );
        xkb_keymap *map = nullptr;
        xkb_state *state = nullptr;
        bool callbackFailed = false;
        int32_t repeatRate = -1, repeatDelay = -1;
        std::vector<uint32_t> enteredKeys;
        uint32_t keyEvents = 0;
        xkb_keysym_t lastSym = XKB_KEY_NoSymbol;
        wl_listener modifiers{};
        wl_listener key{};

        explicit WireKeyboard( bool bOverride = false ) : k{ bOverride }
        {
            REQUIRE( server );
            REQUIRE( context );
            REQUIRE( wlr_compositor_create( server, 4, nullptr ) );
            seat = wlr_seat_create( server, "seat0" );
            REQUIRE( seat );
            wlr_seat_set_capabilities( seat, WL_SEAT_CAPABILITY_KEYBOARD );
            wlr_seat_set_keyboard( seat, &k.keyboard );
            modifiers.notify = []( wl_listener *listener, void * ) {
                WireKeyboard *self = wl_container_of( listener, self, modifiers );
                if ( wlr_seat_get_keyboard( self->seat ) != &self->k.keyboard )
                    return;
                wlr_seat_keyboard_notify_modifiers( self->seat, &self->k.keyboard.modifiers );
            };
            key.notify = []( wl_listener *listener, void *data ) {
                WireKeyboard *self = wl_container_of( listener, self, key );
                auto *event = static_cast<wlr_keyboard_key_event *>( data );
                wlr_seat_set_keyboard( self->seat, &self->k.keyboard );
                wlr_seat_keyboard_notify_key( self->seat, event->time_msec, event->keycode, event->state );
            };
            wl_signal_add( &k.keyboard.events.modifiers, &modifiers );
            wl_signal_add( &k.keyboard.events.key, &key );
            k.Attach();

            int sockets[2];
            REQUIRE( socketpair( AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets ) == 0 );
            serverClient = wl_client_create( server, sockets[0] );
            REQUIRE( serverClient );
            client = wl_display_connect_to_fd( sockets[1] );
            REQUIRE( client );
            registry = wl_display_get_registry( client );
            static const wl_registry_listener registryListener = {
                .global = []( void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t ) {
                    auto *self = static_cast<WireKeyboard *>( data );
                    if ( !strcmp( interface, "wl_seat" ) )
                        self->clientSeat = static_cast<wl_seat *>( wl_registry_bind( registry, name, &wl_seat_interface, 7 ) );
                    if ( !strcmp( interface, "wl_compositor" ) )
                        self->clientCompositor = static_cast<wl_compositor *>( wl_registry_bind( registry, name, &wl_compositor_interface, 4 ) );
                },
                .global_remove = []( void *, wl_registry *, uint32_t ) {},
            };
            wl_registry_add_listener( registry, &registryListener, this );
            Roundtrip();
            REQUIRE( clientSeat );
            REQUIRE( clientCompositor );
            clientKeyboard = wl_seat_get_keyboard( clientSeat );
            static const wl_keyboard_listener keyboardListener = {
                .keymap = []( void *data, wl_keyboard *, uint32_t format, int32_t fd, uint32_t size ) {
                    auto *self = static_cast<WireKeyboard *>( data );
                    if ( format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || size == 0 )
                    {
                        self->callbackFailed = true;
                        close( fd );
                        return;
                    }
                    char *text = static_cast<char *>( mmap( nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0 ) );
                    if ( text == MAP_FAILED )
                    {
                        self->callbackFailed = true;
                        close( fd );
                        return;
                    }
                    xkb_state_unref( self->state );
                    xkb_keymap_unref( self->map );
                    self->map = xkb_keymap_new_from_buffer( self->context, text, size - 1, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS );
                    self->state = self->map ? xkb_state_new( self->map ) : nullptr;
                    self->callbackFailed |= !self->state;
                    munmap( text, size );
                    close( fd );
                },
                .enter = []( void *data, wl_keyboard *, uint32_t, wl_surface *, wl_array *keys ) {
                    auto *self = static_cast<WireKeyboard *>( data );
                    self->enteredKeys.clear();
                    if ( keys->size )
                    {
                        auto *begin = static_cast<uint32_t *>( keys->data );
                        self->enteredKeys.assign( begin, begin + keys->size / sizeof(uint32_t) );
                    }
                },
                .leave = []( void *, wl_keyboard *, uint32_t, wl_surface * ) {},
                .key = []( void *data, wl_keyboard *, uint32_t, uint32_t, uint32_t key, uint32_t state ) {
                    auto *self = static_cast<WireKeyboard *>( data );
                    ++self->keyEvents;
                    if ( !self->state )
                    {
                        self->callbackFailed = true;
                        return;
                    }
                    if ( state == WL_KEYBOARD_KEY_STATE_PRESSED )
                        self->lastSym = xkb_state_key_get_one_sym( self->state, key + 8 );
                },
                .modifiers = []( void *data, wl_keyboard *, uint32_t, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group ) {
                    auto *self = static_cast<WireKeyboard *>( data );
                    if ( !self->state )
                    {
                        self->callbackFailed = true;
                        return;
                    }
                    xkb_state_update_mask( self->state, depressed, latched, locked, 0, 0, group );
                },
                .repeat_info = []( void *data, wl_keyboard *, int32_t rate, int32_t delay ) {
                    auto *self = static_cast<WireKeyboard *>( data );
                    self->repeatRate = rate;
                    self->repeatDelay = delay;
                },
            };
            wl_keyboard_add_listener( clientKeyboard, &keyboardListener, this );
            clientSurface = wl_compositor_create_surface( clientCompositor );
            Roundtrip();
            auto *resource = wl_client_get_object( serverClient, wl_proxy_get_id( reinterpret_cast<wl_proxy *>( clientSurface ) ) );
            REQUIRE( resource );
            wlr_seat_keyboard_notify_enter( seat, wlr_surface_from_resource( resource ), nullptr, 0, &k.keyboard.modifiers );
            Roundtrip();
        }
        ~WireKeyboard()
        {
            wl_list_remove( &modifiers.link );
            wl_list_remove( &key.link );
            wl_surface_destroy( clientSurface );
            wl_keyboard_destroy( clientKeyboard );
            wl_seat_destroy( clientSeat );
            wl_compositor_destroy( clientCompositor );
            wl_registry_destroy( registry );
            wl_display_disconnect( client );
            wl_display_destroy_clients( server );
            wl_display_destroy( server );
            xkb_state_unref( state );
            xkb_keymap_unref( map );
            xkb_context_unref( context );
        }
        void Roundtrip()
        {
            bool done = false;
            wl_callback *callback = wl_display_sync( client );
            static const wl_callback_listener listener = { .done = []( void *data, wl_callback *callback, uint32_t ) {
                *static_cast<bool *>( data ) = true;
                wl_callback_destroy( callback );
            } };
            wl_callback_add_listener( callback, &listener, &done );
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
            while ( !done && std::chrono::steady_clock::now() < deadline )
            {
                int ret = wl_display_flush( client );
                REQUIRE( (ret >= 0 || errno == EAGAIN) );
                REQUIRE( wl_event_loop_dispatch( wl_display_get_event_loop( server ), 0 ) >= 0 );
                wl_display_flush_clients( server );
                REQUIRE( wl_display_dispatch_pending( client ) >= 0 );
                if ( done )
                    break;
                REQUIRE( wl_display_prepare_read( client ) == 0 );
                pollfd fd = { .fd = wl_display_get_fd( client ), .events = POLLIN };
                ret = poll( &fd, 1, 1 );
                if ( ret > 0 )
                    ret = wl_display_read_events( client );
                else
                    wl_display_cancel_read( client );
                REQUIRE( ret >= 0 );
                REQUIRE( wl_display_dispatch_pending( client ) >= 0 );
            }
            REQUIRE_FALSE( callbackFailed );
            REQUIRE( done );
        }

        void Press( uint32_t code )
        {
            k.host.Key( code, true, 1 );
            k.host.Key( code, false, 2 );
            Roundtrip();
        }
    };
}

TEST_CASE( "A Wayland client receives host maps, layout state and each key once", "[host_keyboard][wayland]" )
{
    WireKeyboard wire;
    wire.k.Map( "us,de" );
    wire.k.host.SetModifiers( 0, 0, 0, 1 );
    wire.Roundtrip();
    wire.Press( KEY_Y );
    REQUIRE( wire.lastSym == XKB_KEY_z );
    REQUIRE( wire.keyEvents == 2 );
    wire.k.host.SetModifiers( 0, 0, 0, 0 );
    wire.Roundtrip();
    wire.Press( KEY_Y );
    REQUIRE( wire.lastSym == XKB_KEY_y );
    REQUIRE( wire.keyEvents == 4 );
    wire.k.Map( "fr" );
    wire.k.host.SetModifiers( 0, 0, 0, 0 );
    wire.Roundtrip();
    wire.Press( KEY_Q );
    REQUIRE( wire.lastSym == XKB_KEY_a );
    REQUIRE( wire.keyEvents == 6 );
}

TEST_CASE( "A Wayland client with an override receives locally derived Shift", "[host_keyboard][wayland]" )
{
    WireKeyboard wire{ true };
    wire.k.Map( "de" );
    wire.k.host.SetModifiers( ~0u, 0, ~0u, 1 );
    wire.k.host.Key( KEY_LEFTSHIFT, true, 1 );
    wire.Roundtrip();
    wire.Press( KEY_Y );
    REQUIRE( wire.lastSym == XKB_KEY_Y );
    wire.k.host.Key( KEY_LEFTSHIFT, false, 3 );
    wire.Roundtrip();
    wire.Press( KEY_Y );
    REQUIRE( wire.lastSym == XKB_KEY_y );
}

TEST_CASE( "A fresh client keymap is followed by state even with Shift still held", "[host_keyboard][wayland]" )
{
    WireKeyboard wire;
    wire.k.Map( "us" );
    uint32_t shift = 1u << xkb_keymap_mod_get_index( wire.k.keyboard.keymap, "Shift" );
    wire.k.host.Key( KEY_LEFTSHIFT, true, 1 );
    wire.k.host.SetModifiers( shift, 0, 0, 0 );
    wire.Roundtrip();
    wire.k.Map( "de" );
    wire.Roundtrip();
    wire.Press( KEY_Y );
    REQUIRE( wire.lastSym == XKB_KEY_Z );
}


TEST_CASE( "A transferred keymap owns its private context after the producer exits", "[host_keyboard]" )
{
    Keyboard k;
    k.Attach();
    gamescope::XkbKeymap map;
    std::thread producer( [&] {
        xkb_context *context = xkb_context_new( XKB_CONTEXT_NO_ENVIRONMENT_NAMES );
        if ( !context )
            return;
        xkb_rule_names rules = { .layout = "de" };
        map.reset( xkb_keymap_new_from_names( context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS ) );
        xkb_context_unref( context );
    } );
    producer.join();
    REQUIRE( map );
    REQUIRE( k.host.SetKeymap( std::move( map ) ) );
    REQUIRE_FALSE( map );
    REQUIRE( k.Sym( KEY_Y ) == XKB_KEY_z );
}

TEST_CASE( "A map swap retains locks and active layout without a new modifiers event", "[host_keyboard][wayland]" )
{
    WireKeyboard wire;
    wire.k.Map( "us,de" );
    uint32_t locks = (1u << xkb_keymap_mod_get_index( wire.k.keyboard.keymap, "Lock" )) |
        (1u << xkb_keymap_mod_get_index( wire.k.keyboard.keymap, "Mod2" ));
    wire.k.host.SetModifiers( 0, 0, locks, 1 );
    wire.Roundtrip();
    wire.k.Map( "fr,de" );
    wire.Roundtrip();
    REQUIRE( xkb_state_serialize_mods( wire.state, XKB_STATE_MODS_LOCKED ) == locks );
    REQUIRE( xkb_state_serialize_layout( wire.state, XKB_STATE_LAYOUT_EFFECTIVE ) == 1 );
    wire.Press( KEY_Y );
    REQUIRE( wire.lastSym == XKB_KEY_Z );
}

TEST_CASE( "Host repeat settings survive startup and can disable client repeat", "[host_keyboard][wayland]" )
{
    Keyboard k;
    k.host.SetRepeatInfo( 40, 300 );
    k.Attach();
    REQUIRE( k.keyboard.repeat_info.rate == 40 );
    REQUIRE( k.keyboard.repeat_info.delay == 300 );
    WireKeyboard wire;
    wire.k.host.SetRepeatInfo( 0, 400 );
    wire.Roundtrip();
    REQUIRE( wire.repeatRate == 0 );
    REQUIRE( wire.repeatDelay == 400 );
}

// Contract coverage of wlroots enter arrays, not Gamescope's focus routing.
TEST_CASE( "wlroots focus contract preserves held keys and balanced releases", "[host_keyboard][wayland][contract]" )
{
    WireKeyboard wire;
    wl_surface *other = wl_compositor_create_surface( wire.clientCompositor );
    wire.Roundtrip();
    wire.k.host.Key( KEY_LEFTCTRL, true, 1 );
    wire.k.host.Key( KEY_A, true, 2 );
    auto focus = [&]( wl_surface *surface ) {
        auto *resource = wl_client_get_object( wire.serverClient, wl_proxy_get_id( reinterpret_cast<wl_proxy *>( surface ) ) );
        REQUIRE( resource );
        wlr_seat_keyboard_notify_enter( wire.seat, wlr_surface_from_resource( resource ),
            wire.k.keyboard.keycodes, wire.k.keyboard.num_keycodes, &wire.k.keyboard.modifiers );
        wire.Roundtrip();
    };
    focus( other );
    focus( wire.clientSurface );
    REQUIRE( wire.enteredKeys == std::vector<uint32_t>{ KEY_LEFTCTRL, KEY_A } );
    wire.k.host.Key( KEY_A, false, 3 );
    wire.k.host.Key( KEY_LEFTCTRL, false, 4 );
    wire.Roundtrip();
    REQUIRE( wire.k.keyboard.num_keycodes == 0 );
    REQUIRE( wire.keyEvents == 4 );
    wl_surface_destroy( other );
}

// The fixture models the filter; this does not call wlserver_handle_host_modifiers.
TEST_CASE( "IME selection contract defers host modifiers until host input resumes", "[host_keyboard][wayland][contract]" )
{
    WireKeyboard wire;
    wlr_keyboard ime{};
    wlr_keyboard_init( &ime, nullptr, "test-ime" );
    auto map = Keymap( "us" );
    REQUIRE( wlr_keyboard_set_keymap( &ime, map.get() ) );
    wlr_seat_set_keyboard( wire.seat, &ime );
    wire.k.Map( "de" );
    wire.k.host.SetModifiers( 1, 0, 0, 0 );
    wire.Roundtrip();
    REQUIRE( wlr_seat_get_keyboard( wire.seat ) == &ime );
    wire.Press( KEY_Y );
    REQUIRE( wlr_seat_get_keyboard( wire.seat ) == &wire.k.keyboard );
    REQUIRE( wire.lastSym == XKB_KEY_Z );
    wlr_keyboard_finish( &ime );
}
