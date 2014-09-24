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

#include "Program.hpp"
#include "Shader.hpp"
#include "Error.hpp"

#include <exceptions.hpp>

#include <memory>

#include <GLES2/gl2.h>


Program::Program()
{
}


Program::~Program()
{
	if( this->id )
		glDeleteProgram( this->id );
}


void Program::create()
{
	GLES2_ERROR_CHECK_UNHANDLED();
	if( this->id )
	{
		glDeleteProgram( this->id );
		GLES2_ERROR_CHECK("glDeleteProgram");
	}
	this->id = glCreateProgram();
	GLES2_ERROR_CHECK("glCreateProgram");
}


void Program::attach( const Shader & shader )
{
	GLES2_ERROR_CHECK_UNHANDLED();
	glAttachShader( this->id, shader.getID() );
	GLES2_ERROR_CHECK("glAttachShader");
}


void Program::link()
{
	GLES2_ERROR_CHECK_UNHANDLED();
	glLinkProgram( this->id );
	GLES2_ERROR_CHECK("glLinkProgram");

	int isLinked;
	glGetProgramiv( this->id, GL_LINK_STATUS, &isLinked );
	if( !isLinked )
	{
		GLint infoLen = 0;
		glGetProgramiv( this->id, GL_INFO_LOG_LENGTH, &infoLen );

		if( infoLen > 1 )
		{
			std::unique_ptr< GLchar[] > infoLog( new GLchar[infoLen] );
			glGetProgramInfoLog( this->id, infoLen, NULL, infoLog.get() );
			GLES2_ERROR_CHECK("glGetProgramInfoLog");
			std::string log( infoLog.get(), infoLen-1 );
			glDeleteProgram( this->id );
			throw RUNTIME_ERROR
			(
				"Error linking shader program:\n"
				"----------------\n"
				+log+"\n"
				"----------------\n"
			);
		}
		else
		{
			glDeleteProgram( this->id );
			throw RUNTIME_ERROR( "Error linking shader program! (no log generated)\n" );
		}
	}
}


GLint Program::getAttributeLocation( const std::string & name ) const
{
	GLES2_ERROR_CHECK_UNHANDLED();
	GLint location = glGetAttribLocation( this->id, name.c_str() );
	GLES2_ERROR_CHECK("glGetAttribLocation");
	return location;
}


GLint Program::getUniformLocation( const std::string & name ) const
{
	GLES2_ERROR_CHECK_UNHANDLED();
	GLint location = glGetUniformLocation( this->id, name.c_str() );
	GLES2_ERROR_CHECK("glGetUniformLocation");
	return location;
}
