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

#include <iostream>
#include <string>
#include <stdexcept>
#include <utility>
#include <map>
#include <cmath>
#include <unistd.h>

#include <getopt.h>

#include <SDL.h>
#include <SDL_opengles2.h>

#include <IL/il.h>

#include "exceptions.hpp"
#include "Program.hpp"
#include "Shader.hpp"
#include "Error.hpp"


#define SDL2_ERROR( what ) \
	std::runtime_error( std::string(__PRETTY_FUNCTION__) + std::string(": ") + (what) + std::string(": ") + SDL_GetError() )


static const char * vertexShaderSRC_copy =
R"GLSL(#version 100
varying vec2 vTexCoord;

attribute vec2 aPosition;
attribute vec2 aTexCoord;

void main()
{
	gl_Position = vec4( aPosition, 0.0, 1.0 );
	vTexCoord = aTexCoord;
}
)GLSL";


static const char * fragmentShaderSRC_copy =
R"GLSL(#version 100
varying lowp vec2 vTexCoord;

uniform sampler2D uTexture;

void main()
{
	gl_FragColor = texture2D( uTexture, vTexCoord );
}
)GLSL";


static const char * vertexShaderSRC_water =
R"GLSL(#version 100
varying vec2 vTexCoord;

attribute vec2 aPosition;
attribute vec2 aTexCoord;

void main()
{
	gl_Position = vec4( aPosition, 0.0, 1.0 );
	vTexCoord = aTexCoord;
}
)GLSL";


// inspired by http://madebyevan.com/webgl-water/water.js
static const char * fragmentShaderSRC_water =
R"GLSL(#version 100
varying lowp vec2 vTexCoord;

uniform sampler2D uTexture;
uniform lowp vec2 uDeltaPixel;

void main()
{
	lowp vec4 tex = texture2D( uTexture, vTexCoord );

	// convert unsigned texture data to signed data
	lowp float height = tex.r - 0.5;
	lowp float velocity = tex.g - 0.5;

	// calculate average neighbor height
	lowp vec2 dcx = vec2( uDeltaPixel.x, 0.0 );
	lowp vec2 dcy = vec2( 0.0, uDeltaPixel.y );
	lowp float leftHeight = texture2D( uTexture, vTexCoord - dcx ).r;
	lowp float rightHeight = texture2D( uTexture, vTexCoord + dcx ).r;
	lowp float belowHeight = texture2D( uTexture, vTexCoord - dcy ).r;
	lowp float aboveHeight = texture2D( uTexture, vTexCoord + dcy ).r;
	lowp float averageHeight = 0.25 * ( leftHeight + rightHeight + belowHeight + aboveHeight );
	averageHeight -= 0.5; // unsigned to signed

	// change the velocity to move toward the average
	velocity += (averageHeight - height) * 1.6;

	// change the velocity to move toward zero water level
	velocity -= height * 0.06;

	// attenuate the velocity a little so waves do not last forever
	velocity *= 0.98;

	// update current height
	height += velocity;

	// convert signed data back to unsigned data
	tex.r = height + 0.5;
	tex.g = velocity + 0.5;

	// height difference in x and y direction
	tex.b = (rightHeight - leftHeight) + 0.5;
	tex.a = (aboveHeight - belowHeight) + 0.5;

	gl_FragColor = tex;
}
)GLSL";


static const char * vertexShaderSRC_waterDrawer =
R"GLSL(#version 100
varying vec2 vTexCoord;

attribute vec2 aPosition;
attribute vec2 aTexCoord;

void main()
{
	gl_Position = vec4( aPosition, 0.0, 1.0 );
	vTexCoord = aTexCoord;
}
)GLSL";


static const char * fragmentShaderSRC_waterDrawer =
R"GLSL(#version 100
varying lowp vec2 vTexCoord;

uniform sampler2D uWaterTexture;
uniform sampler2D uBackgroundTexture;

void main()
{
	lowp vec4 water = texture2D( uWaterTexture, vTexCoord );
	lowp vec2 offset = vec2( water.b-0.5, water.a-0.5 ) * 0.04;
	lowp vec4 background = texture2D( uBackgroundTexture, vTexCoord + offset );
	gl_FragColor = background;
}
)GLSL";


static const char * vertexShaderSRC_waterModulator =
R"GLSL(#version 100
varying vec4 vColor;

attribute vec2 aPosition;
attribute vec4 aColor;

uniform vec2 uPosition;
uniform vec2 uScale;

void main()
{
	gl_Position = vec4( (aPosition*uScale) + uPosition, 0.0, 1.0 );
	vColor = aColor;
}
)GLSL";


static const char * fragmentShaderSRC_waterModulator =
R"GLSL(#version 100
varying lowp vec4 vColor;

void main()
{
	gl_FragColor = vColor;
}
)GLSL";


struct VertexPT
{
	float position[2];
	float texCoord[2];
};

static VertexPT centeredQuadPT[4] =
{
	{ {-1,-1 }, { 0, 0 } },
	{ { 1,-1 }, { 1, 0 } },
	{ {-1, 1 }, { 0, 1 } },
	{ { 1, 1 }, { 1, 1 } }
};


struct VertexPC
{
	float position[2];
	float color[4];
};

static VertexPC centeredCirclePC[8];


struct Touch
{
	float point[2];
	uint8_t r;
	uint8_t g;
	uint8_t b;
};
std::map< int64_t, Touch > touches;


SDL_Window * window = nullptr;
SDL_GLContext glContext = nullptr;

Program program_waterDrawer;
GLint program_waterDrawer_aPosition;
GLint program_waterDrawer_aTexCoord;
GLint program_waterDrawer_uWaterTexture;
GLint program_waterDrawer_uBackgroundTexture;

Program program_waterModulator;
GLint program_waterModulator_aPosition;
GLint program_waterModulator_aColor;
GLint program_waterModulator_uPosition;
GLint program_waterModulator_uScale;

Program program_water;
GLint program_water_aPosition;
GLint program_water_aTexCoord;
GLint program_water_uTexture;
GLint program_water_uDeltaPixel;

Program program_copy;
GLint program_copy_aPosition;
GLint program_copy_aTexCoord;
GLint program_copy_uTexture;

GLuint vertexBufferCenteredQuadPT;
GLuint vertexBufferCenteredCirclePC;
GLuint frameBuffer[2];
GLuint texture[2];
GLuint backgroundTexture;


void render_water( GLuint sourceTexture, unsigned int width, unsigned int height )
{
	program_water.use();
	glUniform2f( program_water_uDeltaPixel, 1.0/width, 1.0/height );
	glUniform1i( program_water_uTexture, 0 );
	glBindTexture( GL_TEXTURE_2D, sourceTexture );

	glBindBuffer( GL_ARRAY_BUFFER, vertexBufferCenteredQuadPT );
	glVertexAttribPointer( program_water_aPosition, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPT), (void*)offsetof(VertexPT,position) );
	glVertexAttribPointer( program_water_aTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPT), (void*)offsetof(VertexPT,texCoord) );
	glEnableVertexAttribArray( program_water_aPosition );
	glEnableVertexAttribArray( program_water_aTexCoord );
	glDrawArrays( GL_TRIANGLE_STRIP, 0, sizeof(centeredQuadPT)/sizeof(VertexPT) );
}


void render_waterDrawer( GLuint waterTexture, GLuint backgroundTexture )
{
	program_waterDrawer.use();

	glActiveTexture( GL_TEXTURE0 );
	glUniform1i( program_waterDrawer_uWaterTexture, 0 );
	glBindTexture( GL_TEXTURE_2D, waterTexture );
	glActiveTexture( GL_TEXTURE1 );
	glUniform1i( program_waterDrawer_uBackgroundTexture, 1);
	glBindTexture( GL_TEXTURE_2D, backgroundTexture );

	glBindBuffer( GL_ARRAY_BUFFER, vertexBufferCenteredQuadPT );
	glVertexAttribPointer( program_waterDrawer_aPosition, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPT), (void*)offsetof(VertexPT,position) );
	glVertexAttribPointer( program_waterDrawer_aTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPT), (void*)offsetof(VertexPT,texCoord) );
	glEnableVertexAttribArray( program_waterDrawer_aPosition );
	glEnableVertexAttribArray( program_waterDrawer_aTexCoord );
	glDrawArrays( GL_TRIANGLE_STRIP, 0, sizeof(centeredQuadPT)/sizeof(VertexPT) );

	glActiveTexture( GL_TEXTURE0 );
}


void render_copy( GLuint texture )
{
	program_copy.use();

	glActiveTexture( GL_TEXTURE0 );
	glUniform1i( program_copy_uTexture, 0 );
	glBindTexture( GL_TEXTURE_2D, texture );

	glBindBuffer( GL_ARRAY_BUFFER, vertexBufferCenteredQuadPT );
	glVertexAttribPointer( program_copy_aPosition, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPT), (void*)offsetof(VertexPT,position) );
	glVertexAttribPointer( program_copy_aTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPT), (void*)offsetof(VertexPT,texCoord) );
	glEnableVertexAttribArray( program_copy_aPosition );
	glEnableVertexAttribArray( program_copy_aTexCoord );
	glDrawArrays( GL_TRIANGLE_STRIP, 0, sizeof(centeredQuadPT)/sizeof(VertexPT) );

	glActiveTexture( GL_TEXTURE0 );
}


void render_waterModulator( float position[2], float scale )
{
	program_waterModulator.use();
	glUniform2fv( program_waterModulator_uPosition, 1, position );
	glUniform2f( program_waterModulator_uScale, scale, scale );

	glBindBuffer( GL_ARRAY_BUFFER, vertexBufferCenteredCirclePC );
	glVertexAttribPointer( program_waterModulator_aPosition, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPC), (void*)offsetof(VertexPC,position) );
	glVertexAttribPointer( program_waterModulator_aColor, 4, GL_FLOAT, GL_FALSE, sizeof(VertexPC), (void*)offsetof(VertexPC,color) );
	glEnableVertexAttribArray( program_waterModulator_aPosition );
	glEnableVertexAttribArray( program_waterModulator_aColor );
	glDrawArrays( GL_TRIANGLE_FAN, 0, sizeof(centeredCirclePC)/sizeof(VertexPC) );
}


struct arguments
{
	std::string backgroundImageFile;
	unsigned int waterResolution;
};


void print_usage( int argc, char ** argv )
{
	printf
	(
		"Usage: %s [-r=int] [--resolution=int] <background image file>\n",
		argv[0]
	);
}


int main( int argc, char ** argv )
{
	////////////////////////////////
	// argument parsing
	struct arguments arguments;
	arguments.waterResolution = 256;

	static struct option long_options[] =
	{
		{ "resolution",  required_argument, 0, 'r' },
		{ 0,         0,                     0, 0   }
	};

	int opt = 0;
	int option_index = 0;
	while( ( opt = getopt_long( argc, argv, "r:", long_options, &option_index ) ) != -1 )
	{
		switch( opt )
		{
		case 'r':
			arguments.waterResolution = strtoul( optarg, NULL, 10 );
			break;
		default:
			print_usage( argc, argv );
			return EXIT_FAILURE;
		}
	}

	if( optind+1 != argc )
	{
		fprintf( stderr, "Need a background image file!\n" );
		print_usage( argc, argv );
		return EXIT_FAILURE;
	}
	else
	{
		arguments.backgroundImageFile = argv[optind++];
	}
	////////////////////////////////

	////////////////////////////////
	// Initialisation
	ilInit();
	ilEnable( IL_ORIGIN_SET );
	ilOriginFunc( IL_ORIGIN_LOWER_LEFT );

	//SDL_LogSetAllPriority( SDL_LOG_PRIORITY_DEBUG );
	SDL_Init( SDL_INIT_VIDEO | SDL_INIT_EVENTS );

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );

	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );

	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

	window = SDL_CreateWindow(
		"glesPond",             // window title
		SDL_WINDOWPOS_CENTERED, // the x position of the window
		SDL_WINDOWPOS_CENTERED, // the y position of the window
		arguments.waterResolution, arguments.waterResolution, // window width and height
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
	);
	if( !window )
		throw SDL2_ERROR( "Could not create window" );

	SDL_DisplayMode mode;
	SDL_GetCurrentDisplayMode( 0, &mode );
	std::cout << "Screen      : " << SDL_BITSPERPIXEL(mode.format) << "BPP " << mode.w << "x" << mode.h << "@" << mode.refresh_rate << "Hz\n";

	glContext = SDL_GL_CreateContext( window );
	if( !glContext )
		throw SDL2_ERROR( "Could not create OpenGL context" );

	SDL_GL_MakeCurrent( window, glContext );
	SDL_GL_SetSwapInterval( 1 );

	glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	glClearDepthf( 1.0f );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	std::cout << "Vendor      : " << glGetString(GL_VENDOR) << "\n";
	std::cout << "Renderer    : " << glGetString(GL_RENDERER) << "\n";
	std::cout << "Version     : " << glGetString(GL_VERSION) << "\n";
	std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";
	std::cout << "Extensions  : " << glGetString(GL_EXTENSIONS) << "\n";
	////////////////////////////////

	////////////////////////////////
	// Geometry
	for( unsigned int i=0; i<sizeof(centeredCirclePC)/sizeof(VertexPC); i++ )
	{
		centeredCirclePC[i].position[0] = std::sin( (2.0*M_PI/ (sizeof(centeredCirclePC)/sizeof(VertexPC)) )*i );
		centeredCirclePC[i].position[1] = std::cos( (2.0*M_PI/ (sizeof(centeredCirclePC)/sizeof(VertexPC)) )*i );
		centeredCirclePC[i].color[0] = 0.0; // position
		centeredCirclePC[i].color[1] = 0.5; // velocity (unchanged)
		centeredCirclePC[i].color[2] = 0.5; // dx (unchanged)
		centeredCirclePC[i].color[3] = 0.5; // dy (unchanged)
	}
	////////////////////////////////

	////////////////////////////////
	// VBOs
	glGenBuffers( 1, &vertexBufferCenteredQuadPT);
	glBindBuffer( GL_ARRAY_BUFFER, vertexBufferCenteredQuadPT );
	glBufferData( GL_ARRAY_BUFFER, sizeof(centeredQuadPT), centeredQuadPT, GL_STATIC_DRAW );

	glGenBuffers( 1, &vertexBufferCenteredCirclePC);
	glBindBuffer( GL_ARRAY_BUFFER, vertexBufferCenteredCirclePC );
	glBufferData( GL_ARRAY_BUFFER, sizeof(centeredCirclePC), centeredCirclePC, GL_STATIC_DRAW );
	////////////////////////////////

	////////////////////////////////
	// Shaders
	program_waterDrawer.create();
	program_waterDrawer.attach( Shader( GL_VERTEX_SHADER, vertexShaderSRC_waterDrawer ) );
	program_waterDrawer.attach( Shader( GL_FRAGMENT_SHADER, fragmentShaderSRC_waterDrawer ) );
	program_waterDrawer.link();
	program_waterDrawer_aPosition = program_waterDrawer.getAttributeLocation( "aPosition" );
	if( program_waterDrawer_aPosition == -1 )
		throw RUNTIME_ERROR( "Attribute aPosition not found in shader!" );
	program_waterDrawer_aTexCoord = program_waterDrawer.getAttributeLocation( "aTexCoord" );
	if( program_waterDrawer_aTexCoord == -1 )
		throw RUNTIME_ERROR( "Attribute aTexCoord not found in shader!" );
	program_waterDrawer_uWaterTexture = program_waterDrawer.getUniformLocation( "uWaterTexture" );
	if( program_waterDrawer_uWaterTexture == -1 )
		throw RUNTIME_ERROR( "Uniform uWaterTexture not found in shader!" );
	program_waterDrawer_uBackgroundTexture = program_waterDrawer.getUniformLocation( "uBackgroundTexture" );
	if( program_waterDrawer_uBackgroundTexture == -1 )
		throw RUNTIME_ERROR( "Uniform uBackgroundTexture not found in shader!" );

	program_water.create();
	program_water.attach( Shader( GL_VERTEX_SHADER, vertexShaderSRC_water ) );
	program_water.attach( Shader( GL_FRAGMENT_SHADER, fragmentShaderSRC_water ) );
	program_water.link();
	program_water_aPosition = program_water.getAttributeLocation( "aPosition" );
	if( program_water_aPosition == -1 )
		throw RUNTIME_ERROR( "Attribute aPosition not found in shader!" );
	program_water_aTexCoord = program_water.getAttributeLocation( "aTexCoord" );
	if( program_water_aTexCoord == -1 )
		throw RUNTIME_ERROR( "Attribute aTexCoord not found in shader!" );
	program_water_uTexture = program_water.getUniformLocation( "uTexture" );
	if( program_water_uTexture == -1 )
		throw RUNTIME_ERROR( "Uniform uTexture not found in shader!" );
	program_water_uDeltaPixel = program_water.getUniformLocation( "uDeltaPixel" );
	if( program_water_uDeltaPixel == -1 )
		throw RUNTIME_ERROR( "Uniform uDeltaPixel not found in shader!" );

	program_waterModulator.create();
	program_waterModulator.attach( Shader( GL_VERTEX_SHADER, vertexShaderSRC_waterModulator ) );
	program_waterModulator.attach( Shader( GL_FRAGMENT_SHADER, fragmentShaderSRC_waterModulator ) );
	program_waterModulator.link();
	program_waterModulator_aPosition = program_waterModulator.getAttributeLocation( "aPosition" );
	if( program_waterModulator_aPosition == -1 )
		throw RUNTIME_ERROR( "Attribute aPosition not found in shader!" );
	program_waterModulator_aColor = program_waterModulator.getAttributeLocation( "aColor" );
	if( program_waterModulator_aColor == -1 )
		throw RUNTIME_ERROR( "Attribute aColor not found in shader!" );
	program_waterModulator_uPosition = program_waterModulator.getUniformLocation( "uPosition" );
	if( program_waterModulator_uPosition == -1 )
		throw RUNTIME_ERROR( "Uniform uPosition not found in shader!" );
	program_waterModulator_uScale = program_waterModulator.getUniformLocation( "uScale" );
	if( program_waterModulator_uScale == -1 )
		throw RUNTIME_ERROR( "Uniform uScale not found in shader!" );

	program_copy.create();
	program_copy.attach( Shader( GL_VERTEX_SHADER, vertexShaderSRC_copy ) );
	program_copy.attach( Shader( GL_FRAGMENT_SHADER, fragmentShaderSRC_copy ) );
	program_copy.link();
	program_copy_aPosition = program_copy.getAttributeLocation( "aPosition" );
	if( program_copy_aPosition == -1 )
		throw RUNTIME_ERROR( "Attribute aPosition not found in shader!" );
	program_copy_aTexCoord = program_copy.getAttributeLocation( "aTexCoord" );
	if( program_copy_aTexCoord == -1 )
		throw RUNTIME_ERROR( "Attribute aTexCoord not found in shader!" );
	program_copy_uTexture = program_copy.getUniformLocation( "uTexture" );
	if( program_copy_uTexture == -1 )
		throw RUNTIME_ERROR( "Uniform uTexture not found in shader!" );
	////////////////////////////////

	////////////////////////////////
	// Framebuffer Textures
	glGenFramebuffers( 2, frameBuffer );
	glGenTextures( 2, texture );
	glClearColor( 0.5f, 0.5f, 0.5f, 0.5f );

	glBindTexture( GL_TEXTURE_2D, texture[0] );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, arguments.waterResolution, arguments.waterResolution, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D,  GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D,  GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glBindFramebuffer( GL_FRAMEBUFFER, frameBuffer[0] );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[0], 0 );
	if( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		throw RUNTIME_ERROR( "Framebuffer is not complete!" );
	glClear( GL_COLOR_BUFFER_BIT );

	glBindTexture( GL_TEXTURE_2D, texture[1] );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, arguments.waterResolution, arguments.waterResolution, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D,  GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D,  GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glBindFramebuffer( GL_FRAMEBUFFER, frameBuffer[1] );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[1], 0 );
	if( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		throw RUNTIME_ERROR( "Framebuffer is not complete!" );
	glClear( GL_COLOR_BUFFER_BIT );

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	////////////////////////////////

	////////////////////////////////
	// Textures
	ILuint image;
	ilGenImages( 1, &image );
	ilBindImage( image );
	ilLoadImage( arguments.backgroundImageFile.c_str() );
	ilConvertImage( IL_RGB, IL_UNSIGNED_BYTE );

	glGenTextures( 1, &backgroundTexture );
	glBindTexture( GL_TEXTURE_2D, backgroundTexture );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, ilGetInteger( IL_IMAGE_WIDTH ), ilGetInteger( IL_IMAGE_HEIGHT ), 0, GL_RGB, GL_UNSIGNED_BYTE, (void*)ilGetData() );

	ilDeleteImages( 1, &image );
	////////////////////////////////

	bool quit = false;
	while( !quit )
	{
		int w = 0, h = 0;
		SDL_GetWindowSize( window, &w, &h );

		SDL_Event sdlEvent;
		while( SDL_PollEvent( &sdlEvent ) )
		{
			switch( sdlEvent.type )
			{
			case SDL_QUIT:
				quit = true;
				break;
			case SDL_MOUSEBUTTONDOWN:
				{
					Touch t;
					t.point[0] = (sdlEvent.button.x/(float)w)*2.0-1.0f;
					t.point[1] = -((sdlEvent.button.y/(float)h)*2.0-1.0f);
					t.r = rand() % 128 + 127;
					t.g = rand() % 128 + 127;
					t.b = rand() % 128 + 127;
					touches[ -1 ] = t;
				}
				break;
			case SDL_MOUSEMOTION:
				{
					auto i = touches.find( -1 );
					if( i != touches.end() )
					{
						Touch & t = i->second;
						t.point[0] = (sdlEvent.motion.x/(float)w)*2.0-1.0f;
						t.point[1] = -((sdlEvent.motion.y/(float)h)*2.0-1.0f);
					}
				}
				break;
			case SDL_MOUSEBUTTONUP:
				touches.erase( -1 );
				break;
			case SDL_FINGERDOWN:
				{
					Touch t;
					t.point[0] = (sdlEvent.tfinger.x/(float)w)*2.0-1.0f;
					t.point[1] = -((sdlEvent.tfinger.y/(float)h)*2.0-1.0f);
					t.r = rand() % 128 + 127;
					t.g = rand() % 128 + 127;
					t.b = rand() % 128 + 127;
					touches[ sdlEvent.tfinger.fingerId ] = t;
				}
				break;
			case SDL_FINGERUP:
				touches.erase( sdlEvent.tfinger.fingerId );
				break;
			case SDL_FINGERMOTION:
				{
					auto i = touches.find( sdlEvent.tfinger.fingerId );
					if( i != touches.end() )
					{
						Touch & t = i->second;
						t.point[0] = (sdlEvent.tfinger.x/(float)w)*2.0-1.0f;
						t.point[1] = -((sdlEvent.tfinger.y/(float)h)*2.0-1.0f);
					}
				}
				break;
			case SDL_KEYDOWN:
				switch( sdlEvent.key.keysym.sym )
				{
				case SDLK_ESCAPE:
					quit = true;
					break;
				case SDLK_RETURN:
					if( SDL_GetModState() & KMOD_ALT )
					{
						static Uint32 lastFullscreenFlags = SDL_WINDOW_FULLSCREEN_DESKTOP;
						Uint32 fullscreenFlags = SDL_GetWindowFlags( window ) & ( SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP );
						if( fullscreenFlags )
						{
							lastFullscreenFlags = fullscreenFlags;
							fullscreenFlags = 0;
						}
						else
						{
							fullscreenFlags = lastFullscreenFlags;
						}
						SDL_SetWindowFullscreen( window, fullscreenFlags );
					}
					break;
				}
				break;
			}
		}

		glBindFramebuffer( GL_FRAMEBUFFER, frameBuffer[1] );
		glViewport( 0, 0, arguments.waterResolution, arguments.waterResolution );
		for( auto t : touches )
		{
			render_waterModulator( t.second.point, 0.03f );
		}

		glBindFramebuffer( GL_FRAMEBUFFER, frameBuffer[0] );
		glViewport( 0, 0, arguments.waterResolution, arguments.waterResolution );
		render_water( texture[1], arguments.waterResolution, arguments.waterResolution );

		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		glViewport( 0, 0, w, h );
		render_waterDrawer( texture[0], backgroundTexture );

		std::swap( texture[0], texture[1] );
		std::swap( frameBuffer[0], frameBuffer[1] );
		SDL_GL_SwapWindow( window );
	}

	SDL_Quit();

	return 0;
}
