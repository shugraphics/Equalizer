
/* Copyright (c) 2005-2007, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#ifndef EQBASE_REFPTR_H
#define EQBASE_REFPTR_H

#include <eq/base/base.h>
#include <eq/base/debug.h>

#include <iostream>
#include <stdlib.h>

namespace eqBase
{
    struct RefPtr_scast{};

    /**
     * A smart reference pointer
     */
    template<class T> class RefPtr 
    {
    public:
        RefPtr()                   : _ptr(NULL)      {}
        RefPtr(T* ptr)             : _ptr(ptr)       { ref(); }
        RefPtr(const RefPtr& from) : _ptr(from._ptr) { ref(); }
        
        template<class from>
        RefPtr( RefPtr<from> const &f, RefPtr_scast )
            { _ptr = static_cast<T*>(f._ptr); ref(); }


        ~RefPtr() { unref(); }

        void ref()   { if(_ptr) _ptr->ref(); }
        void unref() { if(_ptr) _ptr->unref(); }

        RefPtr& operator = ( const RefPtr& rhs )
            {
                if( _ptr == rhs._ptr )
                    return *this;

                T* tmp = _ptr;
                _ptr = rhs._ptr;
                ref();
                if( tmp ) tmp->unref();
                return *this;
            }
        RefPtr& operator = ( T* ptr )
            {
                if( _ptr == ptr )
                    return *this;

                T* tmp = _ptr;
                _ptr = ptr;
                ref();
                if( tmp ) tmp->unref();
                return *this;
            }

        bool operator == ( const RefPtr& rhs ) const 
            { return ( _ptr==rhs._ptr ); }
        bool operator != ( const RefPtr& rhs ) const
            { return ( _ptr!=rhs._ptr ); }
        bool operator <  ( const RefPtr& rhs ) const
            { return ( _ptr < rhs._ptr ); }
        bool operator >  ( const RefPtr& rhs ) const
            { return ( _ptr > rhs._ptr ); }
        bool operator ! () const               { return ( _ptr==NULL ); }

        bool operator == ( const T* ptr ) const { return ( _ptr==ptr ); }
        bool operator != ( const T* ptr ) const { return ( _ptr!=ptr ); }
        bool operator <  ( const T* ptr ) const { return ( _ptr < ptr ); }
        bool operator >  ( const T* ptr ) const { return ( _ptr > ptr ); }

        T*       operator->()       
            { EQASSERTINFO( _ptr, typeid(*this).name( )); return _ptr; }
        const T* operator->() const
            { EQASSERTINFO( _ptr, typeid(*this).name( )); return _ptr; }
        T&       operator*()        
            { EQASSERTINFO( _ptr, typeid(*this).name( )); return *_ptr; }
        const T& operator*() const  
            { EQASSERTINFO( _ptr, typeid(*this).name( )); return *_ptr; }
        T*       get()              { return _ptr; }
        const T* get() const        { return _ptr; }

        bool isValid() { return ( _ptr!=NULL ); }
        
    private:
        T* _ptr;

        template<class U> friend class RefPtr;
    };

    // cast functions
    template<class from, class to> RefPtr<to> RefPtr_static_cast( RefPtr<from>
                                                                  const &f )
    { return RefPtr<to>( f, RefPtr_scast() ); }

    template< class T >
    inline std::ostream& operator << ( std::ostream& os, const RefPtr<T>& rp )
    {
        os << "RefPtr<" << rp.get() << ">";
        return os;
    }
}

#endif //EQBASE_REFPTR_H
