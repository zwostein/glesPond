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

#ifndef _TEXTURE2D_INCLUDED_
#define _TEXTURE2D_INCLUDED_


#include "Error.hpp"

#include <string>

#include <GLES2/gl2.h>


class Texture2D
{
public:
	Texture2D( const Texture2D & ) = delete;
	Texture2D & operator=( const Texture2D & ) = delete;

	Texture2D( unsigned int width, unsigned int height, GLint internalFormat, GLint minFilter, GLint maxFilter, GLint wrapS, GLint wrapT );
	Texture2D( unsigned int width, unsigned int height, GLint internalFormat )
		: Texture2D( width, height, internalFormat, GL_NEAREST, GL_LINEAR, GL_REPEAT, GL_REPEAT )
	{}

	Texture2D( const std::string & file, GLint minFilter, GLint maxFilter, GLint wrapS, GLint wrapT );
	Texture2D( const std::string & file )
		: Texture2D( file, GL_NEAREST, GL_LINEAR, GL_REPEAT, GL_REPEAT )
	{}

	virtual ~Texture2D();

	void bind() const
	{
		GLES2_ERROR_CHECK_UNHANDLED();
		glBindTexture( GL_TEXTURE_2D, this->id );
		GLES2_ERROR_CHECK("glBindTexture");
	}

	void bind( unsigned int unit ) const
	{
		GLES2_ERROR_CHECK_UNHANDLED();
		glActiveTexture( GL_TEXTURE0 + unit );
		GLES2_ERROR_CHECK("glActiveTexture");
		glBindTexture( GL_TEXTURE_2D, this->id );
		GLES2_ERROR_CHECK("glBindTexture");
	}

	const GLuint & getID() const
	{
		return this->id;
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
	unsigned int width = 0;
	unsigned int height = 0;
};


#endif
