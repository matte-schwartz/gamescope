#include "waitable.h"
#include <string_view>
#include <string>
#include <ranges>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengles2.h>

#include "../thirdparty/imgui/imgui.h"
#include "../thirdparty/imgui/backends/imgui_impl_sdl3.h"
#include "../thirdparty/imgui/backends/imgui_impl_opengl3.h"
#include <systemd/sd-journal.h>

namespace gamescope
{
    static constexpr uint32_t k_uMaxMatches = 9;

    class GamescopeHUD
    {
    public:
        GamescopeHUD();
        ~GamescopeHUD();

        bool Init();
        void Shutdown();

        bool Frame();

        void DrawConsole();
        void DrawNotify();

        void TextEditCallback( ImGuiInputTextCallbackData *data );
        void Submit();
    private:
        SDL_Window *m_pWindow = nullptr;
        SDL_GLContext m_pGlContext = nullptr;
        sd_journal *m_pSystemdJournal = nullptr;
        int m_nSystemdFd = -1;

        char m_szEditLine[ 256 ];
        bool m_bOpen = true;
        bool m_bWordWrap = true;
        bool m_bCompletionPopup = false;
        bool m_bScrollToBottom = false;
        bool m_bIgnoreEdit = false;
        std::string m_szConsoleBuffer{ "Big Frog" };
        int m_nCompletionPosition = -1;
    };

    GamescopeHUD::GamescopeHUD()
    {
    }

    GamescopeHUD::~GamescopeHUD()
    {
        Shutdown();
    }

    bool GamescopeHUD::Init()
    {
        // SDL
        if ( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER ) != 0 )
        {
            fprintf( stderr, "Failed to init SDL: %s\n", SDL_GetError() );
            return false;
        }

        // GL ES 2.0 + GLSL 100
        const char* glsl_version = "#version 100";
        SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );
        SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
        SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
        SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
        SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
        SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
        SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
        SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
        SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

        m_pWindow = SDL_CreateWindow(
            "Gamescope HUD",
            1280, 720,
            SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_TRANSPARENT );
        if ( !m_pWindow )
        {
            fprintf( stderr, "Failed to create SDL window: %s\n", SDL_GetError() );
            return false;
        }
        SDL_SetWindowPosition( m_pWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED );

        m_pGlContext = SDL_GL_CreateContext( m_pWindow );
        if ( !m_pGlContext )
        {
            fprintf( stderr, "Failed to create GL context\n" );
            return false;
        }
        SDL_GL_MakeCurrent( m_pWindow, m_pGlContext );
        SDL_GL_SetSwapInterval( 0 ); // Mailbox
        SDL_ShowWindow( m_pWindow );

        // ImGUI
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr; // Disable imgui.ini
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsLight();

        // Setup Platform/Renderer backends
        ImGui_ImplSDL3_InitForOpenGL( m_pWindow, m_pGlContext );
        ImGui_ImplOpenGL3_Init( glsl_version );

        // SystemD
        int nRet = sd_journal_open( &m_pSystemdJournal, SD_JOURNAL_LOCAL_ONLY );
        if ( nRet < 0 )
        {
            fprintf( stderr, "Failed to open SystemD journal.\n" );
            return false;
        }

        m_nSystemdFd = sd_journal_get_fd( m_pSystemdJournal );
        if ( m_nSystemdFd < 0 )
        {
            fprintf( stderr, "Failed to get SystemD journal fd.\n" );
            return false;
        }

        return true;
    }

    void GamescopeHUD::Shutdown()
    {
        if ( m_pSystemdJournal )
            sd_journal_close( m_pSystemdJournal );

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        SDL_DestroyWindow( m_pWindow );
        SDL_GL_DeleteContext( m_pGlContext );
        SDL_Quit();
    }

    bool GamescopeHUD::Frame()
    {
        {
            SDL_Event event;
            while ( SDL_PollEvent( &event ) )
            {
                ImGui_ImplSDL3_ProcessEvent(&event);

                if ( event.type == SDL_EVENT_QUIT )
                    return false;

                if ( event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                     event.window.windowID == SDL_GetWindowID( m_pWindow ) )
                    return false;
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        DrawNotify();

        // Rendering
        ImGuiIO& io = ImGui::GetIO();
        ImGui::Render();
        glViewport( 0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y );
        glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
        glClear( GL_COLOR_BUFFER_BIT );
        ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
        SDL_GL_SwapWindow( m_pWindow );

        return true;
    }

    void GamescopeHUD::DrawConsole()
    {
        bool *pOpen = &m_bOpen;
        assert( *pOpen );

        ImGui::PushStyleVar( ImGuiStyleVar_WindowMinSize, ImVec2( 330.0f, 250.0f ) );

        ImGui::SetNextWindowSize( ImVec2( 600.0f, 800.0f ), ImGuiCond_FirstUseEver );
        if ( !ImGui::Begin( "Console", pOpen ) )
        {
            ImGui::PopStyleVar();
            ImGui::End();
            return;
        }

        ImGui::PopStyleVar();

        if ( ImGui::BeginPopupContextItem() )
        {
            if ( ImGui::MenuItem( "Close Console" ) )
            {
                *pOpen = false;
            }
            ImGui::EndPopup();
        }

        // save the old wordwrap state
        bool wordWrap = m_bWordWrap;

        ImGuiWindowFlags scrollFlags = wordWrap ? 0 : ImGuiWindowFlags_HorizontalScrollbar;

        // Reserve enough left-over height for 1 separator + 1 input text
        ImGui::BeginChild( "ScrollingRegion", ImVec2( 0.0f, -ImGui::GetFrameHeightWithSpacing() ), true, scrollFlags );

        if ( ImGui::BeginPopupContextWindow() )
        {
            ImGui::Checkbox( "Word wrap", &m_bWordWrap );
            ImGui::EndPopup();
        }

        if ( wordWrap )
        {
            ImGui::PushTextWrapPos( 0.0f );
        }

        if ( !m_szConsoleBuffer.empty() )
        {
            const char *bufferStart = m_szConsoleBuffer.c_str();
            const char *bufferEnd = m_szConsoleBuffer.c_str() + m_szConsoleBuffer.size() - 1;

            const int bufferSize = (int)m_szConsoleBuffer.size() - 1; // skip nul

            if ( bufferSize > 16384 )
            {
                // draw the buffer in 16384 chunks so we can avoid over-running the max vertex count of 65536
                // (16384 * 4 == 65536)
                const char *newStart = bufferStart;

                int chunkSize = 16384;
                int amountLeft = bufferSize;

                ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2( 0.0f, 0.0f ) );

                while ( true )
                {
                    // starting from the end of the chunk we're supposed to be drawing now, step back to find a newline
                    // then use that as the end instead, and subsequently the start of the next chunk
                    // this goes under the assumption that there's a newline somewhere... With more than 16384 chars
                    // in the buffer this will *always* be true

                    const char *newEnd = newStart + chunkSize;

                    for ( const char *i = newEnd; i > newStart; --i )
                    {
                        if ( *i == '\n' )
                        {
                            chunkSize -= ( chunkSize - static_cast<int>( i - newStart ) ) - 1;
                            newEnd = i;
                            break;
                        }
                    }

                    ImGui::TextUnformatted( newStart, newEnd );

                    amountLeft -= chunkSize;
                    newStart += chunkSize;

                    if ( amountLeft <= 0 )
                    {
                        break;
                    }

                    if ( amountLeft < 16384 )
                    {
                        chunkSize = amountLeft;
                    }
                }

                ImGui::PopStyleVar();
            }
            else
            {
                ImGui::TextUnformatted( bufferStart, bufferEnd );
            }
        }

        if ( wordWrap )
        {
            ImGui::PopTextWrapPos();
        }

        if ( m_bScrollToBottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY() )
        {
            ImGui::SetScrollHereY( 1.0f );
        }

        m_bScrollToBottom = false;

        ImGui::EndChild();

        // input line
        const ImGuiInputTextFlags inputFlags =
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion |
            ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackAlways;

        // steal focus from the window upon opening, no matter what
        bool focusOnInput = ImGui::IsWindowAppearing() ? true : false;

        ImGui::PushItemWidth( ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x - 64.0f );

        if ( ImGui::InputText( "##Input", m_szEditLine, sizeof( m_szEditLine ), inputFlags, []( ImGuiInputTextCallbackData *pData ){ ((GamescopeHUD *)pData->UserData)->TextEditCallback( pData ); }, this ) )
        {
            Submit();
            focusOnInput = true;
        }

        ImGui::PopItemWidth();

        // set the input text as the default item
        ImGui::SetItemDefaultFocus();

        if ( focusOnInput )
        {
            ImGui::SetKeyboardFocusHere( -1 );
        }

        // store the bottom left position of the input window, then add the spacing X Y
        const ImVec2 popupPosition( ImGui::GetItemRectMin().x - ImGui::GetStyle().ItemSpacing.x, ImGui::GetItemRectMax().y + ImGui::GetStyle().ItemSpacing.y + 5.0f );

        ImGui::SameLine();

        if ( ImGui::Button( "Submit" ) )
        {
            Submit();
            //focusOnInput = true;
        }

        //if ( focusOnInput )
        //{
        //	ImGui::SetKeyboardFocusHere( -1 );
        //}

        if ( m_bCompletionPopup )
        {
            const ImGuiWindowFlags popFlags =
                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize;

            if ( !m_bIgnoreEdit )
            {
                RegenerateMatches( m_szEditLine );
            }

#if 0
            if ( con.entryMatches.size() != 0 )
            {
                ImGui::SetNextWindowPos( popupPosition );

                // the command popup should only list the first n entries, starting with commands
                if ( ImGui::Begin( "CommandPopup", nullptr, popFlags ) )
                {
                    int i = 0;

                    // loop for the cmds, they're printed green
                    ImGui::PushStyleColor( ImGuiCol_Text, CmdColor );
                    for ( ; i < con.beginCvars && i < k_uMaxMatches; ++i )
                    {
                        ImGui::Selectable( con.entryMatches[i].data() );
                    }
                    ImGui::PopStyleColor();

                    char workBuf[256];

                    // loop for the cvars
                    for ( ; i < (int)con.entryMatches.size() && i < k_uMaxMatches; ++i )
                    {
                        // HACK: This sucks a little
                        Q_sprintf_s( workBuf, "%s %s", con.entryMatches[i].data(), Cvar_FindGetString( con.entryMatches[i].data() ) );
                        ImGui::Selectable( workBuf );
                    }

                    if ( i >= k_uMaxMatches )
                    {
                        ImGui::Selectable( "...", false, ImGuiSelectableFlags_Disabled );
                    }
                }

                ImGui::End();
            }
#endif
        }
        /*else
        {
            // TODO: show history?
        }*/

        ImGui::End();
    }

    void GamescopeHUD::TextEditCallback( ImGuiInputTextCallbackData *data )
    {
        switch ( data->EventFlag )
        {
        case ImGuiInputTextFlags_CallbackEdit:
        {
            if ( !m_bIgnoreEdit && data->BufTextLen != 0 )
            {
                m_nCompletionPosition = -1;
                m_bCompletionPopup = true;
            }
            m_bIgnoreEdit = false;
        }
        break;
        case ImGuiInputTextFlags_CallbackCompletion:
        {
            const char *cmd = Cmd_CompleteCommand( data->Buf );
            if ( !cmd ) {
                cmd = Cvar_CompleteVariable( data->Buf );
            }

            if ( cmd )
            {
                // replace entire buffer
                data->DeleteChars( 0, data->BufTextLen );
                data->InsertChars( data->CursorPos, cmd );
                data->InsertChars( data->CursorPos, " " );
            }
        }
        break;
        case ImGuiInputTextFlags_CallbackHistory:
        {
            if ( m_bCompletionPopup )
            {
                m_bIgnoreEdit = true;

                switch ( data->EventKey )
                {
                case ImGuiKey_UpArrow:
                    if ( m_nCompletionPosition == -1 ) {
                        // Start at the end
                        m_nCompletionPosition = (int64)con.entryMatches.size() - 1;
                    }
                    else if ( m_nCompletionPosition > 0 ) {
                        // Decrement
                        --m_nCompletionPosition;
                    }
                    break;
                case ImGuiKey_DownArrow:
                    if ( m_nCompletionPosition < (int64)con.entryMatches.size() ) {
                        ++m_nCompletionPosition;
                    }
                    if ( m_nCompletionPosition == (int64)con.entryMatches.size() ) {
                        --m_nCompletionPosition;
                    }
                    break;
                }

                data->DeleteChars( 0, data->BufTextLen );
                const char *match_str = m_nCompletionPosition >= 0 ? con.entryMatches[m_nCompletionPosition].data() : "";
                if ( match_str[0] != '\0' )
                {
                    strlen_t match_len = Q_strlen( match_str );
                    Assert( match_len > 0 );
                    data->InsertChars( data->CursorPos, match_str, match_str + match_len );
                    // If we don't already end with a space, add one to ease typing numbers
                    if ( match_str[match_len - 1] != ' ' )
                    {
                        data->InsertChars( data->CursorPos, " " );
                    }
                }
            }
            else if ( !con.historyLines.empty() )
            {
                switch ( data->EventKey )
                {
                case ImGuiKey_UpArrow:
                    if ( con.historyPosition == -1 ) {
                        // Start at the end
                        con.historyPosition = (int64)con.historyLines.size() - 1;
                    }
                    else if ( con.historyPosition > 0 ) {
                        // Decrement
                        --con.historyPosition;
                    }
                    break;
                case ImGuiKey_DownArrow:
                    if ( con.historyPosition < (int64)con.historyLines.size() ) {
                        ++con.historyPosition;
                    }
                    if ( con.historyPosition == (int64)con.historyLines.size() ) {
                        --con.historyPosition;
                    }
                    break;
                }

                const char *history_str = con.historyPosition >= 0 ? con.historyLines[con.historyPosition].data : "";
                data->DeleteChars( 0, data->BufTextLen );
                data->InsertChars( data->CursorPos, history_str );
            }
        }
        break;
        }

        return 0;
    }

    void GamescopeHUD::Submit()
    {

    }

    void GamescopeHUD::DrawNotify()
    {
        int nRet = sd_journal_process( m_pSystemdJournal );
        if ( nRet < 0 )
        {
            fprintf( stderr, "Failed to process SystemD journal.\n" );
            return;
        }

        std::vector<std::string_view> pszLines;
        static constexpr uint32_t k_uEntriesVisible = 16;
        
        // Seek to the end
        sd_journal_seek_tail( m_pSystemdJournal );
        for ( uint32_t i = 0; i < k_uEntriesVisible; i++ )
        {
            nRet = sd_journal_previous( m_pSystemdJournal );
            if ( nRet < 0 )
                break;

            const char *pLine = nullptr;
            size_t uzLineLength = 0;
            nRet = sd_journal_get_data( m_pSystemdJournal, "MESSAGE", (const void **)&pLine, &uzLineLength );
            if ( nRet < 0 )
                continue;

            pszLines.emplace_back( std::string_view{ pLine, uzLineLength } );
        }

        // Draw notify
        const ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove;

        ImGui::SetNextWindowPos( ImVec2( 4.0f, 8.0f ), ImGuiCond_Always );
        ImGui::SetNextWindowBgAlpha( 0.25f );
        ImGui::SetNextWindowSizeConstraints( ImVec2( 0.0f, 0.0f ), ImVec2( 1280.0f * (2.0f / 3.0f ), 800 / 2.0f ) );
	    ImGui::Begin( "Notify Area", nullptr, windowFlags );
        for ( std::string_view pszLine : std::ranges::reverse_view( pszLines ) )
        {
            ImGui::TextUnformatted( pszLine.begin() + std::string_view{"MESSAGE="}.length(), pszLine.end() );
        }
        ImGui::End();
    }
}

int main()
{
    gamescope::GamescopeHUD console;
    if ( !console.Init() )
        return 1;

    while ( console.Frame() )
    {
    }

    return 0;
}
