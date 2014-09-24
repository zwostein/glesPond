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

#ifndef _ERROR_INCLUDED_
#define _ERROR_INCLUDED_


#include <string>
#include <stdexcept>

#include <GLES2/gl2.h>


#define GLES2_ERROR( error, what ) \
	Error( (error), std::string(__PRETTY_FUNCTION__) + std::string(": ") + (what) )

#ifdef NDEBUG
	#define GLES2_ERROR_CHECK_UNHANDLED()
	#define GLES2_ERROR_CHECK( what )
	#define GLES2_ERROR_CLEAR()
#else
	#define GLES2_ERROR_CHECK_UNHANDLED() \
		Error::check( std::string(__PRETTY_FUNCTION__) + std::string(": Unhandled previous error") )
	#define GLES2_ERROR_CHECK( what ) \
		Error::check( std::string(__PRETTY_FUNCTION__) + std::string(": ") + (what) )
	#define GLES2_ERROR_CLEAR() \
		Error::clear()
#endif


class Error : public std::runtime_error
{
public:
	static std::string getErrorString( GLenum error );

	static void check()
	{
		GLenum error = glGetError();
		if( error )
			throw Error( error );
	}

	static void check( const std::string & what )
	{
		GLenum error = glGetError();
		if( error )
			throw Error( error, what );
	}

	static void clear()
	{
		glGetError();
	}

	Error( GLenum error )
		: std::runtime_error( this->getErrorString( error ) )
	{}

	Error( GLenum error, const std::string & what )
		: std::runtime_error( what + ": " + this->getErrorString( error ) )
	{}

	virtual ~Error() noexcept {}
};


#endif
