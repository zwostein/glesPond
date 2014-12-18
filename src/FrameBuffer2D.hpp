/*
 * Copyright (C) 2014 Tobias Himmer <provisorisch@online.de>
 *
 * This file is part of glesPond.
 *
 * glesPond is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * glesPond is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with glesPond.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _FRAMEBUFFER2D_INCLUDED_
#define _FRAMEBUFFER2D_INCLUDED_


#include "Error.hpp"

#include <string>

#include <GLES2/gl2.h>


class Texture2D;


class FrameBuffer2D
{
public:
	FrameBuffer2D( const FrameBuffer2D & ) = delete;
	FrameBuffer2D & operator=( const FrameBuffer2D & ) = delete;


	FrameBuffer2D( unsigned int width, unsigned int height, GLint internalFormat );

	virtual ~FrameBuffer2D();

	void bind() const
	{
		GLES2_ERROR_CHECK_UNHANDLED();
		glViewport( 0, 0, this->width, this->height );
		GLES2_ERROR_CHECK("glViewport");
		glBindFramebuffer( GL_FRAMEBUFFER, this->id );
		GLES2_ERROR_CHECK("glBindFramebuffer");
	}

	const GLuint & getID() const
	{
		return this->id;
	}

	const Texture2D * getTexture() const
	{
		return this->texture;
	}

	const GLuint & getWidth() const
	{
		return this->width;
	}

	const GLuint & getHeight() const
	{
		return this->height;
	}

private:
	GLuint id = 0;
	Texture2D * texture;
	unsigned int width = 0;
	unsigned int height = 0;
};


#endif
