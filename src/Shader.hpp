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

#ifndef _SHADER_INCLUDED_
#define _SHADER_INCLUDED_


#include <string>

#include <GLES2/gl2.h>


class Shader
{
public:
	Shader( const Shader & ) = delete;
	Shader & operator=( const Shader & ) = delete;

	Shader();
	Shader( GLenum type, const std::string & source );
	virtual ~Shader();

	void load( GLenum type, const std::string & source );
	GLuint getID() const { return this->id; }

private:
	GLuint id = 0;
};


#endif
