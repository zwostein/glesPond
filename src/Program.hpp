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

#ifndef _PROGRAM_INCLUDED_
#define _PROGRAM_INCLUDED_


#include "Shader.hpp"
#include "Error.hpp"

#include <string>

#include <GLES2/gl2.h>


class Program
{
public:
	Program( const Program & ) = delete;
	Program & operator=( const Program & ) = delete;

	Program();
	virtual ~Program();

	void create();
	void attach( const Shader & shader );
	void link();

	GLint getAttributeLocation( const std::string & name ) const;
	GLint getUniformLocation( const std::string & name ) const;

	void use() const
	{
		GLES2_ERROR_CHECK_UNHANDLED();
		glUseProgram( this->id );
		GLES2_ERROR_CHECK("glUseProgram");
	}

private:
	GLuint id = 0;
};


#endif
