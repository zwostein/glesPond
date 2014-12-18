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

#include "Texture2D.hpp"
#include "Error.hpp"

#include <exceptions.hpp>

#include <stdlib.h>

#include <IL/il.h>


Texture2D::Texture2D( unsigned int width, unsigned int height, GLint internalFormat, GLint minFilter, GLint maxFilter, GLint wrapS, GLint wrapT )
{
	GLES2_ERROR_CHECK_UNHANDLED();

	glGenTextures( 1, &this->id );
	GLES2_ERROR_CHECK("glGenTextures");

	glBindTexture( GL_TEXTURE_2D, this->id );
	GLES2_ERROR_CHECK("glBindTexture");

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter );
	GLES2_ERROR_CHECK("glTexParameteri");
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, maxFilter );
	GLES2_ERROR_CHECK("glTexParameteri");

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS );
	GLES2_ERROR_CHECK("glTexParameteri");
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT );
	GLES2_ERROR_CHECK("glTexParameteri");

	glTexImage2D( GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
	GLES2_ERROR_CHECK("glTexImage2D");

	this->width = width;
	this->height = height;
}


Texture2D::Texture2D( const std::string & file, GLint minFilter, GLint maxFilter, GLint wrapS, GLint wrapT )
{
	GLES2_ERROR_CHECK_UNHANDLED();

	ILuint image;
	ilGenImages( 1, &image );
	ilBindImage( image );
	if( !ilLoadImage( file.c_str() ) )
		throw RUNTIME_ERROR( "Could not load \"" + file + "\"!" );

	glGenTextures( 1, &this->id );
	GLES2_ERROR_CHECK("glGenTextures");

	glBindTexture( GL_TEXTURE_2D, this->id );
	GLES2_ERROR_CHECK("glBindTexture");

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter );
	GLES2_ERROR_CHECK("glTexParameteri");
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, maxFilter );
	GLES2_ERROR_CHECK("glTexParameteri");

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS );
	GLES2_ERROR_CHECK("glTexParameteri");
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT );
	GLES2_ERROR_CHECK("glTexParameteri");

	glTexImage2D( GL_TEXTURE_2D, 0, ilGetInteger( IL_IMAGE_FORMAT ), ilGetInteger( IL_IMAGE_WIDTH ), ilGetInteger( IL_IMAGE_HEIGHT ), 0, ilGetInteger( IL_IMAGE_FORMAT ), ilGetInteger( IL_IMAGE_TYPE ), (void*)ilGetData() );
	GLES2_ERROR_CHECK("glTexImage2D");

	this->width = ilGetInteger( IL_IMAGE_WIDTH );
	this->height = ilGetInteger( IL_IMAGE_HEIGHT );

	ilDeleteImages( 1, &image );
}


Texture2D::~Texture2D()
{
	glDeleteTextures( 1, &this->id );
}
