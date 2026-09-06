#pragma once

#include <cstdint>
#include <memory>
#include <xkbcommon/xkbcommon.h>

struct wlr_keyboard;

namespace gamescope
{
    struct XkbKeymapDeleter
    {
        void operator()( xkb_keymap *pKeymap ) const { xkb_keymap_unref( pKeymap ); }
    };
    using XkbKeymap = std::unique_ptr<xkb_keymap, XkbKeymapDeleter>;

    bool HasXkbEnvironmentOverride();

    // Owned by wlserver; all access is serialized by wlserver_lock.
    class CHostKeyboard
    {
    public:
        explicit CHostKeyboard( bool bOverride ) : m_bOverride{ bOverride } {}
        bool SetKeymap( XkbKeymap pKeymap );
        void SetModifiers( uint32_t uDepressed, uint32_t uLatched, uint32_t uLocked, uint32_t uGroup );
        void SetRepeatInfo( int32_t nRate, int32_t nDelay );
        void Leave();
        void Reset();
        bool Attach( wlr_keyboard *pKeyboard );
        void Key( uint32_t uKey, bool bPressed, uint32_t uTime );

    private:
        const bool m_bOverride;
        XkbKeymap m_pKeymap;
        wlr_keyboard *m_pKeyboard = nullptr;
        uint32_t m_uDepressed = 0;
        uint32_t m_uLatched = 0;
        uint32_t m_uLocked = 0;
        uint32_t m_uGroup = 0;
        int32_t m_nRepeatRate = 25;
        int32_t m_nRepeatDelay = 600;
    };
}
