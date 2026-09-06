#include "HostKeyboard.h"

#include <cassert>
#include <cstdlib>

#include "wlr_begin.hpp"
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard.h>
#include "wlr_end.hpp"

namespace gamescope
{
    bool HasXkbEnvironmentOverride()
    {
        // An empty options string is an explicit request for no XKB options.
        return getenv( "XKB_DEFAULT_RULES" ) || getenv( "XKB_DEFAULT_MODEL" ) ||
            getenv( "XKB_DEFAULT_LAYOUT" ) || getenv( "XKB_DEFAULT_VARIANT" ) ||
            getenv( "XKB_DEFAULT_OPTIONS" );
    }

    bool CHostKeyboard::SetKeymap( XkbKeymap pKeymap )
    {
        if ( m_bOverride )
            return true;
        if ( m_pKeyboard && !wlr_keyboard_set_keymap( m_pKeyboard, pKeymap.get() ) )
            return false;
        m_pKeymap = std::move( pKeymap );
        SetModifiers( m_uDepressed, m_uLatched, m_uLocked, m_uGroup );
        // The seat sends a new keymap but does not resend unchanged modifiers.
        if ( m_pKeyboard )
            wl_signal_emit_mutable( &m_pKeyboard->events.modifiers, m_pKeyboard );
        return true;
    }

    bool CHostKeyboard::Attach( wlr_keyboard *pKeyboard )
    {
        assert( !pKeyboard->group );
        m_pKeyboard = pKeyboard;
        SetRepeatInfo( m_nRepeatRate, m_nRepeatDelay );
        return !m_pKeymap || SetKeymap( std::move( m_pKeymap ) );
    }

    void CHostKeyboard::SetModifiers( uint32_t uDepressed, uint32_t uLatched, uint32_t uLocked, uint32_t uGroup )
    {
        m_uDepressed = uDepressed;
        m_uLatched = uLatched;
        m_uLocked = uLocked;
        m_uGroup = uGroup;
        if ( m_pKeymap && m_pKeyboard )
            wlr_keyboard_notify_modifiers( m_pKeyboard, uDepressed, uLatched, uLocked, uGroup );
    }

    void CHostKeyboard::SetRepeatInfo( int32_t nRate, int32_t nDelay )
    {
        m_nRepeatRate = nRate;
        m_nRepeatDelay = nDelay;
        if ( m_pKeyboard )
            wlr_keyboard_set_repeat_info( m_pKeyboard, nRate, nDelay );
    }

    void CHostKeyboard::Leave()
    {
        SetModifiers( 0, 0, m_uLocked, m_uGroup );
    }

    void CHostKeyboard::Reset()
    {
        SetModifiers( 0, 0, 0, 0 );
        m_pKeymap.reset();
    }

    void CHostKeyboard::Key( uint32_t uKey, bool bPressed, uint32_t uTime )
    {
        if ( !m_pKeyboard )
            return;
        wlr_keyboard_key_event event = {
            .time_msec = uTime,
            .keycode = uKey,
            // Host masks already account for lock and layout-switch actions.
            .update_state = !m_pKeymap,
            .state = bPressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED,
        };
        wlr_keyboard_notify_key( m_pKeyboard, &event );
    }
}
