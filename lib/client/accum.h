
/* Copyright (c) 2009, Stefan Eilemann <eile@equalizergraphics.com>
 *                   , Sarah Amsellem <sarah.amsellem@gmail.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *  
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef EQ_ACCUM_H
#define EQ_ACCUM_H

#include <eq/client/os.h>  // for GLEW

namespace eq
{
    class AccumBufferObject;
    class PixelViewport;

    /**
     * A C++ class to abstract an accumulation buffer.
     * Depending on the OpenGL version, an FBO or glAccum()
     * is used.
     *
     * Remark: MacOS systems seem to have a buggy implementation of glAccum().
     */
    class Accum
    {
    public: 
        /** Construct a new accumulation buffer */
        EQ_EXPORT Accum( GLEWContext* const glewContext );

        /** Destruct the accumulation buffer */
        EQ_EXPORT ~Accum();

        /**
         * Inits the accumulation object.
         *
         * @param pvp the pixel viewport.
         * @param textureFormat the texture format.
         */
        EQ_EXPORT bool init( const PixelViewport& pvp, GLuint textureFormat );

        /**
         * Exits the accum object.
         */
        EQ_EXPORT void exit();

        /**
         * Resizes the accumulation object.
         *
         * @param width the new width.
         * @param height the new height.
         * @return true if the accumulation object is correctly resized.
         */
        EQ_EXPORT bool resize( const int width, const int height );

        /**
         * Clears the accumulation object.
         */
        EQ_EXPORT void clear();

        /**
         * Accumulates a frame into the accumulation object.
         */
        EQ_EXPORT void accum();

        /**
         * Displays the result of the accumulation object.
         */
        EQ_EXPORT void display();

        /**
         * Get the number of maximum steps possible to do during the
         * accumulation operation.
         *
         * @return the maximum number of steps.
         */
        EQ_EXPORT uint32_t getMaxSteps() const;

        /**
         * Get the current number of accumulations done.
         *
         * @return the number of steps done.
         */
        EQ_EXPORT uint32_t getNumSteps() const { return _numSteps; }

        /**
         * Set the total steps that will be used.
         *
         * This is needed only for Darwin systems because of the specific
         * glAccum() workaround.
         *
         * @param totalSteps the total number of steps to do.
         */
        EQ_EXPORT void setTotalSteps( uint32_t totalSteps )
            { _totalSteps = totalSteps; }
        EQ_EXPORT uint32_t getTotalSteps() { return _totalSteps; }

        /**
         * Tests if the accumulation uses the FBO implementation.
         *
         * @return true if the accumulation uses an FBO, false if it uses
         *         glAccum().
         */
        EQ_EXPORT bool usesFBO() const;

        /**
         * Tests if the accum buffer has received all contributions.
         * 
         * @return true if NumSteps is greater or equal than TotalSteps.
         */
        EQ_EXPORT bool isFull() const;

        GLEWContext* glewGetContext() { return _glewContext; }
        const GLEWContext* glewGetContext() const { return _glewContext; }

    private:
        GLEWContext* const _glewContext;

        int _width;
        int _height;

        AccumBufferObject* _abo;
        uint32_t _numSteps;
        uint32_t _totalSteps;
    };
}


#endif //EQ_ACCUMULATION_H
