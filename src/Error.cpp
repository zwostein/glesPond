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

#include "Error.hpp"

#include <map>


static std::map< GLenum, std::string > ErrorStrings =
{
	{ GL_NO_ERROR,                      "No Error"                      },
	{ GL_INVALID_ENUM,                  "Invalid Enum"                  },
	{ GL_INVALID_VALUE,                 "Invalid Value"                 },
	{ GL_INVALID_OPERATION,             "Invalid Operation"             },
	{ GL_INVALID_FRAMEBUFFER_OPERATION, "Invalid Framebuffer Operation" },
	{ GL_OUT_OF_MEMORY,                 "Out Of Memory"                 }
};


std::string Error::getErrorString( GLenum error )
{
	auto i = ErrorStrings.find( error );
	if( i == ErrorStrings.end() )
		return "Unknown Error (" + std::to_string( error ) + ")";
	return i->second;
}
