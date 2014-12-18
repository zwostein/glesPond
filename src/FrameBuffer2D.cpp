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

#include "FrameBuffer2D.hpp"
#include "Error.hpp"

#include <exceptions.hpp>
#include "Texture2D.hpp"

#include <stdlib.h>


FrameBuffer2D::FrameBuffer2D( unsigned int width, unsigned int height, GLint internalFormat )
{
	GLES2_ERROR_CHECK_UNHANDLED();

	this->texture = new Texture2D( width, height, internalFormat, GL_NEAREST, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE );

	glGenFramebuffers( 1, &this->id );
	GLES2_ERROR_CHECK("glGenFramebuffers");

	glBindFramebuffer( GL_FRAMEBUFFER, this->id );
	GLES2_ERROR_CHECK("glBindFramebuffer");

	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->texture->getID(), 0 );
	GLES2_ERROR_CHECK("glFramebufferTexture2D");
	if( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		throw RUNTIME_ERROR( "Framebuffer is not complete!" );

	GLfloat oldClearColor[4];
	glGetFloatv( GL_COLOR_CLEAR_VALUE, oldClearColor );
	glClearColor( 0.5f, 0.5f, 0.5f, 0.5f );
	glClear( GL_COLOR_BUFFER_BIT );
	glClearColor( oldClearColor[0], oldClearColor[1], oldClearColor[2], oldClearColor[3] );
	GLES2_ERROR_CHECK("glClearColor");

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	GLES2_ERROR_CHECK("glBindFramebuffer");

	this->width = width;
	this->height = height;
}


FrameBuffer2D::~FrameBuffer2D()
{
	glDeleteFramebuffers( 1, &this->id );
	delete this->texture;
}
