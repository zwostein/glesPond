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

#include "Shader.hpp"
#include "Error.hpp"

#include <exceptions.hpp>

#include <memory>

#include <GLES2/gl2.h>


Shader::Shader()
{
}


Shader::Shader( GLenum type, const std::string & source )
{
	this->load( type, source );
}


Shader::~Shader()
{
	if( this->id )
		glDeleteShader( this->id );
}


void Shader::load( GLenum type, const std::string & source )
{
	GLES2_ERROR_CHECK_UNHANDLED();

	if( this->id )
	{
		glDeleteShader( this->id );
		GLES2_ERROR_CHECK("glDeleteShader");
	}

	GLboolean shaderCompilerAvailable;
	glGetBooleanv( GL_SHADER_COMPILER, &shaderCompilerAvailable );
	if( !shaderCompilerAvailable )
		throw RUNTIME_ERROR( "Shader compiler not available" );

	this->id = glCreateShader( type );
	if( !this->id )
		throw GLES2_ERROR( glGetError(), "glCreateShader" );

	const GLchar * string = source.c_str();
	glShaderSource( this->id, 1, &string, nullptr );
	GLES2_ERROR_CHECK("glShaderSource");

	glCompileShader( this->id );
	GLES2_ERROR_CHECK("glCompileShader");

	GLint compiled;
	glGetShaderiv( this->id, GL_COMPILE_STATUS, &compiled );
	if( !compiled )
	{
		GLint infoLen = 0;
		glGetShaderiv( this->id, GL_INFO_LOG_LENGTH, &infoLen );
		if( infoLen > 1 )
		{
			std::unique_ptr< GLchar[] > infoLog( new GLchar[infoLen] );
			glGetShaderInfoLog( this->id, infoLen, NULL, infoLog.get() );
			GLES2_ERROR_CHECK("glGetShaderInfoLog");
			std::string log( infoLog.get(), infoLen-1 );
			glDeleteShader( this->id );
			throw RUNTIME_ERROR
			(
				"Error compiling shader:\n"
				"----------------\n"
				+source+"\n"
				"--------\n"
				"Log:\n"
				"--------\n"
				+log+"\n"
				"----------------\n"
			);
		}
		else
		{
			glDeleteShader( this->id );
			throw RUNTIME_ERROR
			(
				"Error compiling shader (no log generated):\n"
				"----------------\n"
				+source+"\n"
				"----------------\n"
			);
		}
	}
}
