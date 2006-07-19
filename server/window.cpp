
/* Copyright (c) 2005-2006, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#include "window.h"

#include "channel.h"
#include "config.h"
#include "pipe.h"

using namespace eqs;
using namespace eqBase;
using namespace std;

void Window::_construct()
{
    _used             = 0;
    _pipe             = NULL;
    _pendingRequestID = EQ_INVALID_ID;
    _state            = STATE_STOPPED;

    registerCommand( eq::CMD_WINDOW_INIT_REPLY, this,
                     reinterpret_cast<CommandFcn>(&eqs::Window::_cmdInitReply));
    registerCommand( eq::CMD_WINDOW_EXIT_REPLY, this, 
                     reinterpret_cast<CommandFcn>(&eqs::Window::_cmdExitReply));
}

Window::Window()
        : eqNet::Object( eq::Object::TYPE_WINDOW, eq::CMD_WINDOW_CUSTOM )
{
    _construct();
}

Window::Window( const Window& from )
        : eqNet::Object( eq::Object::TYPE_WINDOW, eq::CMD_WINDOW_CUSTOM )
{
    _construct();

    _name = from._name;
    _pvp  = from._pvp;
    _vp   = from._vp;

    const uint32_t nChannels = from.nChannels();
    for( uint32_t i=0; i<nChannels; i++ )
    {
        Channel* channel      = from.getChannel(i);
        Channel* channelClone = new Channel( *channel );

        addChannel( channelClone );
    }            
}

void Window::addChannel( Channel* channel )
{
    _channels.push_back( channel ); 
    channel->_window = this; 
}

void Window::refUsed()
{
    _used++;
    if( _pipe ) 
        _pipe->refUsed(); 
}
void Window::unrefUsed()
{
    _used--;
    if( _pipe ) 
        _pipe->unrefUsed(); 
}

//----------------------------------------------------------------------
// viewport
//----------------------------------------------------------------------
void eqs::Window::setPixelViewport( const eq::PixelViewport& pvp )
{
    if( !pvp.isValid( ))
        return;

    _pvp = pvp;
    _vp.invalidate();

    if( !_pipe )
        return;
    
    const eq::PixelViewport& pipePVP = _pipe->getPixelViewport();
    if( pipePVP.isValid( ))
        _vp = pvp / pipePVP;
}

void eqs::Window::setViewport( const eq::Viewport& vp )
{
    if( !vp.isValid( ))
        return;
    
    _vp = vp;
    _pvp.invalidate();

    if( !_pipe )
        return;

    eq::PixelViewport pipePVP = _pipe->getPixelViewport();
    if( pipePVP.isValid( ))
    {
        pipePVP.x = 0;
        pipePVP.y = 0;
        _pvp = pipePVP * vp;
    }
}

//---------------------------------------------------------------------------
// swap barrier operations
//---------------------------------------------------------------------------
void Window::resetSwapBarriers()
{ 
    Node* node = getNode();

    for( vector<eqNet::Barrier*>::iterator iter = _masterSwapBarriers.begin();
         iter != _masterSwapBarriers.end(); ++iter )
            
        node->releaseBarrier( *iter );

    _masterSwapBarriers.clear();
    _swapBarriers.clear();
}

eqNet::Barrier* Window::newSwapBarrier()
{
    Node*           node    = getNode();
    eqNet::Barrier* barrier = node->getBarrier();
    _masterSwapBarriers.push_back( barrier );

    addSwapBarrier( barrier );
    return barrier;
}

void Window::addSwapBarrier( eqNet::Barrier* barrier )
{ 
    barrier->increase();
    _swapBarriers.push_back( barrier );
}

//===========================================================================
// Operations
//===========================================================================

//---------------------------------------------------------------------------
// init
//---------------------------------------------------------------------------
void Window::startInit( const uint32_t initID )
{
    _sendInit( initID );

    Config* config = getConfig();
    eq::WindowCreateChannelPacket createChannelPacket;

    const int nChannels = _channels.size();
    for( int i=0; i<nChannels; ++i )
    {
        Channel* channel = _channels[i];
        if( channel->isUsed( ))
        {
            config->registerObject( channel, (eqNet::Node*)getServer( ));
            createChannelPacket.channelID = channel->getID();
            _send( createChannelPacket );

            channel->startInit( initID );
        }
    }
    _state = STATE_INITIALISING;
}

void Window::_sendInit( const uint32_t initID )
{
    EQASSERT( _pendingRequestID == EQ_INVALID_ID );

    eq::WindowInitPacket packet;
    _pendingRequestID = _requestHandler.registerRequest(); 

    packet.requestID = _pendingRequestID;
    packet.initID    = initID;
    packet.pvp       = _pvp; 
    packet.vp        = _vp;

    _send( packet, getName( ));
}

bool Window::syncInit()
{
    bool success = true;
    const int nChannels = _channels.size();
    for( int i=0; i<nChannels; ++i )
    {
        Channel* channel = _channels[i];
        if( channel->isUsed( ))
            if( !channel->syncInit( ))
                success = false;
    }

    EQASSERT( _pendingRequestID != EQ_INVALID_ID );

    if( !(bool)_requestHandler.waitRequest( _pendingRequestID ))
        success = false;
    _pendingRequestID = EQ_INVALID_ID;

    if( success )
        _state = STATE_RUNNING;
    else
        EQWARN << "Window initialisation failed" << endl;
    return success;
}

//---------------------------------------------------------------------------
// exit
//---------------------------------------------------------------------------
void Window::startExit()
{
    _state = STATE_STOPPING;
    const int nChannels = _channels.size();
    for( int i=0; i<nChannels; ++i )
    {
        Channel* channel = _channels[i];
        if( channel->getState() == Channel::STATE_STOPPED )
            continue;

        channel->startExit();
    }

    _sendExit();
}

void Window::_sendExit()
{
    EQASSERT( _pendingRequestID == EQ_INVALID_ID );

    eq::WindowExitPacket packet;
    _pendingRequestID = _requestHandler.registerRequest(); 
    packet.requestID  = _pendingRequestID;
    _send( packet );
}

bool Window::syncExit()
{
    EQASSERT( _pendingRequestID != EQ_INVALID_ID );

    bool success = (bool)_requestHandler.waitRequest( _pendingRequestID );
    _pendingRequestID = EQ_INVALID_ID;
    
    Config* config = getConfig();
    eq::WindowDestroyChannelPacket destroyChannelPacket;

    const int nChannels = _channels.size();
    for( int i=0; i<nChannels; ++i )
    {
        Channel* channel = _channels[i];
        if( channel->getState() != Channel::STATE_STOPPING )
            continue;

        if( !channel->syncExit( ))
            success = false;

        destroyChannelPacket.channelID = channel->getID();
        _send( destroyChannelPacket );
        config->deregisterObject( channel );
    }

    _state = STATE_STOPPED;
    return success;
}

//---------------------------------------------------------------------------
// update
//---------------------------------------------------------------------------
void Window::update( const uint32_t frameID )
{
    // TODO: make current window
    eq::WindowStartFramePacket startPacket;
    startPacket.frameID     = frameID;
    startPacket.makeCurrent = _pipe->nWindows() > 1 ? true : false;
    _send( startPacket );

    const uint32_t nChannels = this->nChannels();
    for( uint32_t i=0; i<nChannels; i++ )
    {
        Channel* channel = getChannel( i );
        if( channel->isUsed( ))
            channel->update( frameID );
    }

    _updateSwap();

    eq::WindowEndFramePacket endPacket;
    endPacket.frameID = frameID;
    _send( endPacket );
}

void Window::_updateSwap()
{
    for( vector<eqNet::Barrier*>::iterator iter = _masterSwapBarriers.begin();
         iter != _masterSwapBarriers.end(); ++iter )

        (*iter)->commit();

    if( !_swapBarriers.empty())
    {
        eq::WindowFinishPacket packet;
        _send( packet );
    }

    for( vector<eqNet::Barrier*>::iterator iter = _swapBarriers.begin();
         iter != _swapBarriers.end(); ++iter )
    {
        const eqNet::Barrier*   barrier = *iter;
        eq::WindowBarrierPacket packet;

        packet.barrierID      = barrier->getID();
        packet.barrierVersion = barrier->getVersion();
        _send( packet );
    }

    eq::WindowSwapPacket packet;
    _send( packet );
}


//===========================================================================
// command handling
//===========================================================================
eqNet::CommandResult Window::_cmdInitReply( eqNet::Node* node, const eqNet::Packet* pkg )
{
    eq::WindowInitReplyPacket* packet = (eq::WindowInitReplyPacket*)pkg;
    EQINFO << "handle window init reply " << packet << endl;

    if( packet->pvp.isValid( ))
        setPixelViewport( packet->pvp );
    _requestHandler.serveRequest( packet->requestID, (void*)packet->result );
    return eqNet::COMMAND_HANDLED;
}

eqNet::CommandResult Window::_cmdExitReply( eqNet::Node* node, const eqNet::Packet* pkg )
{
    eq::WindowExitReplyPacket* packet = (eq::WindowExitReplyPacket*)pkg;
    EQINFO << "handle window exit reply " << packet << endl;

    _requestHandler.serveRequest( packet->requestID, (void*)true );
    return eqNet::COMMAND_HANDLED;
}


std::ostream& eqs::operator << ( std::ostream& os, const Window* window )
{
    if( !window )
        return os;
    
    os << disableFlush << disableHeader << "window" << endl;
    os << "{" << endl << indent; 

    const std::string& name = window->getName();
    if( name.empty( ))
        os << "name \"window_" << (void*)window << "\"" << endl;
    else
        os << "name \"" << name << "\"" << endl;

    const eq::Viewport& vp  = window->getViewport();
    if( vp.isValid( ))
    {
        if( !vp.isFullScreen( ))
            os << "viewport " << vp << endl;
    }
    else
    {
        const eq::PixelViewport& pvp = window->getPixelViewport();
        if( pvp.isValid( ))
            os << "viewport " << pvp << endl;
    }

    os << endl;
    const uint32_t nChannels = window->nChannels();
    for( uint32_t i=0; i<nChannels; i++ )
        os << window->getChannel(i);

    os << exdent << "}" << endl << enableHeader << enableFlush;
    return os;
}
