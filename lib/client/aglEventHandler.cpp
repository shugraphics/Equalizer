/* Copyright (c) 2007, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#include "aglEventHandler.h"

#include "global.h"
#include "log.h"
#include "window.h"
#include "aglWindow.h"

using namespace eq::base;
using namespace std;

namespace eq
{
AGLEventHandler AGLEventHandler::_handler;

AGLEventHandler* AGLEventHandler::get()
{
    return &_handler;
}

AGLEventHandler::AGLEventHandler()
        : _lastDX( 0 ),
          _lastDY( 0 )
{
}

namespace
{
static const AGLWindowIF* _getAGLWindow( const Window* window )
{
    return static_cast< const AGLWindowIF* >( window->getOSWindow( ));
}
}

void AGLEventHandler::registerWindow( Window* window )
{
    OSWindow* osWindow = window->getOSWindow();
    EQASSERT( osWindow );

    if( !dynamic_cast< AGLWindowIF* >( osWindow ))
    {
        EQWARN << "Window does not use an AGL window" << endl;
        return;
    }

    AGLWindowIF* aglWindow = static_cast< AGLWindowIF* >( osWindow );

    const WindowRef carbonWindow = aglWindow->getCarbonWindow();
    if( !carbonWindow )
    {
        EQWARN
            << "Adding window without native Carbon window to AGL event handler"
            << endl;
        return;
    }
    
    Global::enterCarbon();
    EventHandlerUPP eventHandler = NewEventHandlerUPP( 
        eq::AGLEventHandler::_handleEventUPP );
    EventTypeSpec   eventType[]    = {
        { kEventClassWindow,   kEventWindowBoundsChanged },
        { kEventClassWindow,   kEventWindowUpdate },
        { kEventClassWindow,   kEventWindowDrawContent },
        { kEventClassWindow,   kEventWindowClosed },
        { kEventClassMouse,    kEventMouseMoved },
        { kEventClassMouse,    kEventMouseDragged },
        { kEventClassMouse,    kEventMouseDown },
        { kEventClassMouse,    kEventMouseUp },
        { kEventClassKeyboard, kEventRawKeyDown },
        { kEventClassKeyboard, kEventRawKeyUp },
        { kEventClassKeyboard, kEventRawKeyRepeat }
    };

    EventHandlerRef& carbonEventHandler = aglWindow->getCarbonEventHandler();
    InstallWindowEventHandler( carbonWindow, eventHandler, 
                               sizeof( eventType ) / sizeof( EventTypeSpec ),
                               eventType, window, &carbonEventHandler );
    Global::leaveCarbon();
    EQINFO << "Installed event handler for carbon window " << carbonWindow
           << endl;
}

void AGLEventHandler::deregisterWindow( Window* window )
{
    OSWindow* osWindow = window->getOSWindow();
    EQASSERT( osWindow );

    if( !dynamic_cast< AGLWindowIF* >( osWindow ))
    {
        EQWARN << "Window does not use an AGL window" << endl;
        return;
    }

    AGLWindowIF* aglWindow = static_cast< AGLWindowIF* >( osWindow );

    Global::enterCarbon();
    EventHandlerRef& eventHandler = aglWindow->getCarbonEventHandler();
    RemoveEventHandler( eventHandler );
    eventHandler = 0;
    Global::leaveCarbon();
}

pascal OSStatus AGLEventHandler::_handleEventUPP( 
    EventHandlerCallRef nextHandler, EventRef event, void* userData )
{
    Window*          window  = static_cast< eq::Window* >( userData );
    AGLEventHandler* handler = get();

    handler->_handleEvent( event, window );

    // Always pass events to the default handler. Most events require some
    // action, which is not the case on other window systems.
    return CallNextEventHandler( nextHandler, event );
}

bool AGLEventHandler::_handleEvent( EventRef event, eq::Window* window )
{
    switch( GetEventClass( event ))
    {
        case kEventClassWindow:
            return _handleWindowEvent( event, window );
        case kEventClassMouse:
            return _handleMouseEvent( event, window );
        case kEventClassKeyboard:
            return _handleKeyEvent( event, window );
        default:
            EQINFO << "Unknown event class " << GetEventClass( event ) << endl;
            return false;
    }
}

bool AGLEventHandler::_handleWindowEvent( EventRef event, eq::Window* window )
{
    WindowEvent windowEvent;
    windowEvent.carbonEventRef = event;
    windowEvent.window         = window;

    switch( GetEventKind( event ))
    {
        case kEventWindowBoundsChanged:
        {
            Rect rect;
            GetEventParameter( event, kEventParamCurrentBounds, 
                               typeQDRectangle, 0, sizeof( rect ), 0, 
                               &rect );

            windowEvent.data.type = Event::RESIZE;
            windowEvent.data.resize.x = rect.top;
            windowEvent.data.resize.y = rect.left;
            windowEvent.data.resize.h = rect.bottom - rect.top;
            windowEvent.data.resize.w = rect.right  - rect.left;
            break;
        }

        case kEventWindowUpdate:
        {
            const AGLWindowIF* osWindow = _getAGLWindow( window );

            WindowRef carbonWindow = osWindow->getCarbonWindow();
            BeginUpdate( carbonWindow );
            EndUpdate( carbonWindow );
        } // no break;
        case kEventWindowDrawContent:
            windowEvent.data.type = Event::EXPOSE;
            break;

        case kEventWindowClosed:
            windowEvent.data.type = Event::WINDOW_CLOSE;
            break;

        default:
            EQINFO << "Unhandled window event " << GetEventKind( event ) <<endl;
            windowEvent.data.type = Event::UNKNOWN;
            break;
    }
    windowEvent.data.originator = window->getID();

    EQLOG( LOG_EVENTS ) << "received event: " << windowEvent << endl;
    return EventHandler::_processEvent( window, windowEvent );
}

bool AGLEventHandler::_handleMouseEvent( EventRef event, eq::Window* window )
{
    HIPoint     pos;
    WindowEvent windowEvent;

    windowEvent.carbonEventRef = event;
    windowEvent.window         = window;

    const bool    decoration = 
        window->getIAttribute( Window::IATTR_HINT_DECORATION ) != OFF;
    const int32_t menuHeight = decoration ? EQ_AGL_MENUBARHEIGHT : 0 ;

    switch( GetEventKind( event ))
    {
        case kEventMouseMoved:
        case kEventMouseDragged:
            windowEvent.data.type                  = Event::POINTER_MOTION;
            windowEvent.data.pointerMotion.button  = PTR_BUTTON_NONE;
            // Note: Luckily GetCurrentEventButtonState returns the same bits as
            // our button definitions.
            windowEvent.data.pointerMotion.buttons =
                GetCurrentEventButtonState();

            if( windowEvent.data.pointerMotion.buttons == PTR_BUTTON1 )
            {   // Only left button pressed: implement apple-style middle/right
                // button if modifier keys are used.
                uint32_t keys = 0;
                GetEventParameter( event, kEventParamKeyModifiers, 
                                   typeUInt32, 0, sizeof( keys ), 0, &keys );
                if( keys & controlKey )
                    windowEvent.data.pointerMotion.buttons = PTR_BUTTON3;
                else if( keys & optionKey )
                    windowEvent.data.pointerMotion.buttons = PTR_BUTTON2;
            }

            GetEventParameter( event, kEventParamWindowMouseLocation, 
                               typeHIPoint, 0, sizeof( pos ), 0, 
                               &pos );
            if( pos.y < menuHeight )
                return false; // ignore pointer events on the menu bar

            windowEvent.data.pointerMotion.x = static_cast< int32_t >( pos.x );
            windowEvent.data.pointerMotion.y = static_cast< int32_t >( pos.y ) -
                                               menuHeight;

            GetEventParameter( event, kEventParamMouseDelta, 
                               typeHIPoint, 0, sizeof( pos ), 0, 
                               &pos );
            windowEvent.data.pointerMotion.dx = static_cast< int32_t >( pos.x );
            windowEvent.data.pointerMotion.dy = static_cast< int32_t >( pos.y );

            _lastDX = windowEvent.data.pointerMotion.dx;
            _lastDY = windowEvent.data.pointerMotion.dy;

            _getRenderContext( windowEvent );
            break;

        case kEventMouseDown:
            windowEvent.data.type = Event::POINTER_BUTTON_PRESS;
            windowEvent.data.pointerButtonPress.buttons =
                GetCurrentEventButtonState();
            windowEvent.data.pointerButtonPress.button  =
                _getButtonAction( event );

            if( windowEvent.data.pointerMotion.buttons == PTR_BUTTON1 )
            {   // Only left button pressed: implement apple-style middle/right
                // button if modifier keys are used.
                uint32_t keys = 0;
                GetEventParameter( event, kEventParamKeyModifiers, 
                                   typeUInt32, 0, sizeof( keys ), 0, &keys );
                if( keys & controlKey )
                    windowEvent.data.pointerMotion.buttons = PTR_BUTTON3;
                else if( keys & optionKey )
                    windowEvent.data.pointerMotion.buttons = PTR_BUTTON2;
            }

            GetEventParameter( event, kEventParamWindowMouseLocation, 
                               typeHIPoint, 0, sizeof( pos ), 0, 
                               &pos );
            if( pos.y < menuHeight )
                return false; // ignore pointer events on the menu bar

            windowEvent.data.pointerButtonPress.x = 
                static_cast< int32_t >( pos.x );
            windowEvent.data.pointerButtonPress.y = 
                static_cast< int32_t >( pos.y ) - menuHeight;

            windowEvent.data.pointerButtonPress.dx = _lastDX;
            windowEvent.data.pointerButtonPress.dy = _lastDY;
            _lastDX = 0;
            _lastDY = 0;

            _getRenderContext( windowEvent );
            break;

        case kEventMouseUp:
            windowEvent.data.type = Event::POINTER_BUTTON_RELEASE;
            windowEvent.data.pointerButtonRelease.buttons =
                GetCurrentEventButtonState();
            windowEvent.data.pointerButtonRelease.button = 
                _getButtonAction( event );

            if( windowEvent.data.pointerMotion.buttons == PTR_BUTTON1 )
            {   // Only left button pressed: implement apple-style middle/right
                // button if modifier keys are used.
                uint32_t keys = 0;
                GetEventParameter( event, kEventParamKeyModifiers, 
                                   typeUInt32, 0, sizeof( keys ), 0, &keys );
                if( keys & controlKey )
                    windowEvent.data.pointerMotion.buttons = PTR_BUTTON3;
                else if( keys & optionKey )
                    windowEvent.data.pointerMotion.buttons = PTR_BUTTON2;
            }

            GetEventParameter( event, kEventParamWindowMouseLocation, 
                               typeHIPoint, 0, sizeof( pos ), 0, 
                               &pos );
            if( pos.y < menuHeight )
                return false; // ignore pointer events on the menu bar

            windowEvent.data.pointerButtonRelease.x = 
                static_cast< int32_t>( pos.x );
            windowEvent.data.pointerButtonRelease.y = 
                static_cast< int32_t>( pos.y ) - menuHeight;

            windowEvent.data.pointerButtonRelease.dx = _lastDX;
            windowEvent.data.pointerButtonRelease.dy = _lastDY;
            _lastDX = 0;
            _lastDY = 0;

            _getRenderContext( windowEvent );
            break;

        default:
            EQINFO << "Unhandled mouse event " << GetEventKind( event ) << endl;
            windowEvent.data.type = Event::UNKNOWN;
            break;
    }
    windowEvent.data.originator = window->getID();

    EQLOG( LOG_EVENTS ) << "received event: " << windowEvent << endl;
    return EventHandler::_processEvent( window, windowEvent );
}

bool AGLEventHandler::_handleKeyEvent( EventRef event, eq::Window* window )
{
    WindowEvent windowEvent;

    windowEvent.carbonEventRef = event;
    windowEvent.window         = window;

    switch( GetEventKind( event ))
    {
        case kEventRawKeyDown:
        case kEventRawKeyRepeat:
            windowEvent.data.type         = Event::KEY_PRESS;
            windowEvent.data.keyPress.key = _getKey( event );
            break;

        case kEventRawKeyUp:
            windowEvent.data.type         = Event::KEY_RELEASE;
            windowEvent.data.keyPress.key = _getKey( event );
            break;

        default:
            EQINFO << "Unhandled keyboard event " << GetEventKind( event )
                   << endl;
            windowEvent.data.type = Event::UNKNOWN;
            break;
    }
    windowEvent.data.originator = window->getID();

    EQLOG( LOG_EVENTS ) << "received event: " << windowEvent << endl;
    return EventHandler::_processEvent( window, windowEvent );
}

uint32_t AGLEventHandler::_getButtonAction( EventRef event )
{
    EventMouseButton button;
    GetEventParameter( event, kEventParamMouseButton, 
                               typeMouseButton, 0, sizeof( button ), 0, 
                               &button );

    switch( button )
    {    
        case kEventMouseButtonPrimary:   return PTR_BUTTON1;
        case kEventMouseButtonSecondary: return PTR_BUTTON2;
        case kEventMouseButtonTertiary:  return PTR_BUTTON3;
        default: return PTR_BUTTON_NONE;
    }
}

uint32_t AGLEventHandler::_getKey( EventRef event )
{
    unsigned char key;
    GetEventParameter( event, kEventParamKeyMacCharCodes, typeChar, 0,
                       sizeof( char ), 0, &key );
    switch( key )
    {
        case kEscapeCharCode:     return KC_ESCAPE;    
        case kBackspaceCharCode:  return KC_BACKSPACE; 
        case kReturnCharCode:     return KC_RETURN;    
        case kTabCharCode:        return KC_TAB;       
        case kHomeCharCode:       return KC_HOME;       
        case kLeftArrowCharCode:  return KC_LEFT;       
        case kUpArrowCharCode:    return KC_UP;         
        case kRightArrowCharCode: return KC_RIGHT;      
        case kDownArrowCharCode:  return KC_DOWN;       
        case kPageUpCharCode:     return KC_PAGE_UP;    
        case kPageDownCharCode:   return KC_PAGE_DOWN;  
        case kEndCharCode:        return KC_END;        
#if 0
        case XK_F1:        return KC_F1;         
        case XK_F2:        return KC_F2;         
        case XK_F3:        return KC_F3;         
        case XK_F4:        return KC_F4;         
        case XK_F5:        return KC_F5;         
        case XK_F6:        return KC_F6;         
        case XK_F7:        return KC_F7;         
        case XK_F8:        return KC_F8;         
        case XK_F9:        return KC_F9;         
        case XK_F10:       return KC_F10;        
        case XK_F11:       return KC_F11;        
        case XK_F12:       return KC_F12;        
        case XK_F13:       return KC_F13;        
        case XK_F14:       return KC_F14;        
        case XK_F15:       return KC_F15;        
        case XK_F16:       return KC_F16;        
        case XK_F17:       return KC_F17;        
        case XK_F18:       return KC_F18;        
        case XK_F19:       return KC_F19;        
        case XK_F20:       return KC_F20;        
        case XK_Shift_L:   return KC_SHIFT_L;    
        case XK_Shift_R:   return KC_SHIFT_R;    
        case XK_Control_L: return KC_CONTROL_L;  
        case XK_Control_R: return KC_CONTROL_R;  
        case XK_Alt_L:     return KC_ALT_L;      
        case XK_Alt_R:     return KC_ALT_R;
#endif
            
        default: 
            // 'Useful' Latin1 characters
            if(( key >= ' ' && key <= '~' ) ||
               ( key >= 0xa0 /*XK_nobreakspace && key <= XK_ydiaeresis*/ ))

                return key;

            EQWARN << "Unrecognized key " << key << endl;
            return KC_VOID;
    }
}
}
