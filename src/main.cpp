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
#include <vector>
#include <cmath>
#include <cstdlib>
#include <unistd.h>

#include <getopt.h>

#include <SDL.h>
#include <SDL_opengles2.h>

#include <IL/il.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "exceptions.hpp"
#include "Program.hpp"
#include "Shader.hpp"
#include "Texture2D.hpp"
#include "FrameBuffer2D.hpp"
#include "Error.hpp"
#ifdef GLESPOND_POINTIR
	#include "VideoSocketClient.hpp"
	#include "DBusClient.hpp"
#endif


#define SDL2_ERROR( what ) \
	std::runtime_error( std::string(__PRETTY_FUNCTION__) + std::string(": ") + (what) + std::string(": ") + SDL_GetError() )


#ifdef GLESPOND_POINTIR
	static PointIR::DBusClient dbus;
//	static PointIR::VideoSocketClient video;
#endif


constexpr const double PI = 4 * std::atan(1);


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


static const char * vertexShaderSRC_fish =
R"GLSL(#version 100
varying vec2 vTexCoord;

attribute vec2 aPosition;
attribute vec2 aTexCoord;

uniform mat4 uMatrix;

void main()
{
	gl_Position = uMatrix * vec4( aPosition, 0.0, 1.0 );
	vTexCoord = aTexCoord;
}
)GLSL";


static const char * fragmentShaderSRC_fish =
R"GLSL(#version 100
varying lowp vec2 vTexCoord;

uniform sampler2D uTexture;
uniform lowp vec3 uPhaseFreqAmp;

void main()
{
	lowp vec2 coord = vTexCoord;
	coord.s += uPhaseFreqAmp.z * (1.0-coord.t)* (1.0-coord.t) * sin( uPhaseFreqAmp.x + coord.t * uPhaseFreqAmp.y );
	gl_FragColor = texture2D( uTexture, coord );
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


struct Fish
{
	float position[2] = { 0.0f, 0.0f };
	float rotation = 0.0f;
	float scale = 0.075f;
	float phase = 0.0f;
	float agility = 0.004f;
	float speed = 0.0006f;
	float maxSpeed = 0.005f;
	float minSpeed = 0.0005f;
	float rotationSpeed = 0.0f;
	float rotationMaxSpeed = 0.1f;
	float sensitivityDistance = 0.5f;
};
std::vector< Fish > fish;


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

Program program_fish;
GLint program_fish_aPosition;
GLint program_fish_aTexCoord;
GLint program_fish_uTexture;
GLint program_fish_uMatrix;
GLint program_fish_uPhaseFreqAmp;

GLuint vertexBufferCenteredQuadPT;
GLuint vertexBufferCenteredCirclePC;

FrameBuffer2D * waterFrameBufferSrc = nullptr;
FrameBuffer2D * waterFrameBufferDst = nullptr;
FrameBuffer2D * backgroundFrameBuffer = nullptr;

Texture2D * backgroundTexture = nullptr;
Texture2D * fishTexture = nullptr;


float randf()
{
	return (float)rand() / (float)RAND_MAX;
}


void render_water( const Texture2D * sourceTexture, unsigned int width, unsigned int height )
{
	program_water.use();
	glUniform2f( program_water_uDeltaPixel, 1.0/width, 1.0/height );
	glUniform1i( program_water_uTexture, 0 );
	sourceTexture->bind( 0 );

	glBindBuffer( GL_ARRAY_BUFFER, vertexBufferCenteredQuadPT );
	glVertexAttribPointer( program_water_aPosition, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPT), (void*)offsetof(VertexPT,position) );
	glVertexAttribPointer( program_water_aTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPT), (void*)offsetof(VertexPT,texCoord) );
	glEnableVertexAttribArray( program_water_aPosition );
	glEnableVertexAttribArray( program_water_aTexCoord );
	glDrawArrays( GL_TRIANGLE_STRIP, 0, sizeof(centeredQuadPT)/sizeof(VertexPT) );
}


void render_waterDrawer( const Texture2D * waterTexture, const Texture2D * backgroundTexture )
{
	program_waterDrawer.use();
	glUniform1i( program_waterDrawer_uBackgroundTexture, 1);
	backgroundTexture->bind( 1 );
	glUniform1i( program_waterDrawer_uWaterTexture, 0 );
	waterTexture->bind( 0 );

	glBindBuffer( GL_ARRAY_BUFFER, vertexBufferCenteredQuadPT );
	glVertexAttribPointer( program_waterDrawer_aPosition, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPT), (void*)offsetof(VertexPT,position) );
	glVertexAttribPointer( program_waterDrawer_aTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPT), (void*)offsetof(VertexPT,texCoord) );
	glEnableVertexAttribArray( program_waterDrawer_aPosition );
	glEnableVertexAttribArray( program_waterDrawer_aTexCoord );
	glDrawArrays( GL_TRIANGLE_STRIP, 0, sizeof(centeredQuadPT)/sizeof(VertexPT) );
}


void render_copy( const Texture2D * texture )
{
	program_copy.use();
	glUniform1i( program_copy_uTexture, 0 );
	texture->bind( 0 );

	glBindBuffer( GL_ARRAY_BUFFER, vertexBufferCenteredQuadPT );
	glVertexAttribPointer( program_copy_aPosition, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPT), (void*)offsetof(VertexPT,position) );
	glVertexAttribPointer( program_copy_aTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPT), (void*)offsetof(VertexPT,texCoord) );
	glEnableVertexAttribArray( program_copy_aPosition );
	glEnableVertexAttribArray( program_copy_aTexCoord );
	glDrawArrays( GL_TRIANGLE_STRIP, 0, sizeof(centeredQuadPT)/sizeof(VertexPT) );
}


void render_waterModulator( const float position[2], float scale )
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


void render_fish( const std::vector<Fish> & fish )
{
	static float freq = 4.0f;

	static float amp = 0.2f;

	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	program_fish.use();
	glUniform1i( program_fish_uTexture, 0 );
	fishTexture->bind( 0 );


	glBindBuffer( GL_ARRAY_BUFFER, vertexBufferCenteredQuadPT );
	glVertexAttribPointer( program_fish_aPosition, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPT), (void*)offsetof(VertexPT,position) );
	glVertexAttribPointer( program_fish_aTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(VertexPT), (void*)offsetof(VertexPT,texCoord) );
	glEnableVertexAttribArray( program_fish_aPosition );
	glEnableVertexAttribArray( program_fish_aTexCoord );

	for( const auto & f : fish )
	{
		glm::mat4 matrix;
		matrix = glm::translate( matrix, glm::vec3(f.position[0],f.position[1],0.0f) );
		matrix = glm::rotate( matrix, f.rotation, glm::vec3(0.0f,0.0f,1.0f) );
		matrix = glm::scale( matrix, glm::vec3(f.scale,f.scale,f.scale) );

		glUniform3f( program_fish_uPhaseFreqAmp, f.phase, freq, amp );
		glUniformMatrix4fv( program_fish_uMatrix, 1, false, glm::value_ptr(matrix) );

		glDrawArrays( GL_TRIANGLE_STRIP, 0, sizeof(centeredQuadPT)/sizeof(VertexPT) );
	}

	glDisable( GL_BLEND );
}


void update_fish( std::vector<Fish> & fish, const std::map< int64_t, Touch > & touches )
{
	for( auto & f : fish )
	{
		float nearestDistance = std::numeric_limits< float >::max();
		const Touch * nearest = nullptr;
		for( const auto & t : touches )
		{
			float dx = t.second.point[0] - f.position[0];
			float dy = t.second.point[1] - f.position[1];
			float distance = dx*dx + dy*dy;
			if( distance < nearestDistance && distance < f.sensitivityDistance )
			{
				nearestDistance = distance;
				nearest = &t.second;
			}
		}

		glm::vec2 perp( cos(f.rotation), sin(f.rotation) );
		glm::vec2 head( cos(f.rotation+PI/2.0f), sin(f.rotation+PI/2.0f) );
		glm::vec2 toCenter( -f.position[0], -f.position[1] );
		toCenter = glm::normalize( toCenter );

		if( nearest )
		{
			float diff = f.maxSpeed - f.speed;
			f.speed += f.agility * diff;

			glm::vec2 toTouch( nearest->point[0] - f.position[0], nearest->point[1] - f.position[1] );
			toTouch = glm::normalize( toTouch );
			float dot = glm::dot( perp, toTouch );
			f.rotationSpeed -= f.agility * dot;

		}
		else
		{
			float diff = f.speed - f.minSpeed;
			f.speed -= f.agility * diff;

			float dot = glm::dot( perp, toCenter );
			f.rotationSpeed -= 0.1f * f.agility * dot;
		}

		if( f.rotationSpeed > f.rotationMaxSpeed )
			f.rotationSpeed = f.rotationMaxSpeed;
		else if( f.rotationSpeed < -f.rotationMaxSpeed )
			f.rotationSpeed = -f.rotationMaxSpeed;

		f.phase += 5.0f * ((f.speed)/f.scale);
		f.position[0] += head.x * f.speed;
		f.position[1] += head.y * f.speed;
		f.rotation += f.rotationSpeed;
		f.rotationSpeed -= 10.0f * f.agility * f.rotationSpeed;
	}
}


#ifdef GLESPOND_POINTIR
void calibrate()
{
	int w = 0, h = 0;
	SDL_GetWindowSize( window, &w, &h );

	Texture2D calibrationTexture( dbus.getCalibrationImageFile( w, h ) );

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glViewport( 0, 0, w, h );
	render_copy( &calibrationTexture );
	SDL_GL_SwapWindow( window );

	dbus.calibrate();
}
#endif


struct arguments
{
	std::string backgroundImageFile;
	std::string fishTexture;
	unsigned int waterResolutionDivider = 4;
	unsigned int numberOfFish = 0;
};


void print_usage( int argc, char ** argv )
{
	printf
	(
		"Usage: %s [--waterResolutionDivider=int] [--numberOfFish=int] [--fishTexture=string] <background image file>\n",
		argv[0]
	);
}


int main( int argc, char ** argv )
{
	////////////////////////////////
	// argument parsing
	struct arguments arguments;

	static struct option long_options[] =
	{
		{ "waterResolutionDivider", required_argument, 0, 'd' },
		{ "numberOfFish",           required_argument, 0, 'f' },
		{ "fishTexture",            required_argument, 0, 't' },
		{ 0,           0,                              0, 0   }
	};

	int opt = 0;
	int option_index = 0;
	while( ( opt = getopt_long( argc, argv, "d:f:t:", long_options, &option_index ) ) != -1 )
	{
		switch( opt )
		{
		case 'd':
			arguments.waterResolutionDivider = strtoul( optarg, NULL, 10 );
			break;
		case 'f':
			arguments.numberOfFish = strtoul( optarg, NULL, 10 );
			break;
		case 't':
			arguments.fishTexture = optarg;
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

	if( arguments.fishTexture.empty() && arguments.numberOfFish > 0 )
	{
		fprintf( stderr, "Need a fish texture!\n" );
		print_usage( argc, argv );
		return EXIT_FAILURE;
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
		640, 640, // window width and height
		SDL_WINDOW_MAXIMIZED | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
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
	program_waterDrawer_aTexCoord = program_waterDrawer.getAttributeLocation( "aTexCoord" );
	program_waterDrawer_uWaterTexture = program_waterDrawer.getUniformLocation( "uWaterTexture" );
	program_waterDrawer_uBackgroundTexture = program_waterDrawer.getUniformLocation( "uBackgroundTexture" );

	program_water.create();
	program_water.attach( Shader( GL_VERTEX_SHADER, vertexShaderSRC_water ) );
	program_water.attach( Shader( GL_FRAGMENT_SHADER, fragmentShaderSRC_water ) );
	program_water.link();
	program_water_aPosition = program_water.getAttributeLocation( "aPosition" );
	program_water_aTexCoord = program_water.getAttributeLocation( "aTexCoord" );
	program_water_uTexture = program_water.getUniformLocation( "uTexture" );
	program_water_uDeltaPixel = program_water.getUniformLocation( "uDeltaPixel" );

	program_waterModulator.create();
	program_waterModulator.attach( Shader( GL_VERTEX_SHADER, vertexShaderSRC_waterModulator ) );
	program_waterModulator.attach( Shader( GL_FRAGMENT_SHADER, fragmentShaderSRC_waterModulator ) );
	program_waterModulator.link();
	program_waterModulator_aPosition = program_waterModulator.getAttributeLocation( "aPosition" );
	program_waterModulator_aColor = program_waterModulator.getAttributeLocation( "aColor" );
	program_waterModulator_uPosition = program_waterModulator.getUniformLocation( "uPosition" );
	program_waterModulator_uScale = program_waterModulator.getUniformLocation( "uScale" );

	program_copy.create();
	program_copy.attach( Shader( GL_VERTEX_SHADER, vertexShaderSRC_copy ) );
	program_copy.attach( Shader( GL_FRAGMENT_SHADER, fragmentShaderSRC_copy ) );
	program_copy.link();
	program_copy_aPosition = program_copy.getAttributeLocation( "aPosition" );
	program_copy_aTexCoord = program_copy.getAttributeLocation( "aTexCoord" );
	program_copy_uTexture = program_copy.getUniformLocation( "uTexture" );

	program_fish.create();
	program_fish.attach( Shader( GL_VERTEX_SHADER, vertexShaderSRC_fish ) );
	program_fish.attach( Shader( GL_FRAGMENT_SHADER, fragmentShaderSRC_fish ) );
	program_fish.link();
	program_fish_aPosition = program_fish.getAttributeLocation( "aPosition" );
	program_fish_aTexCoord = program_fish.getAttributeLocation( "aTexCoord" );
	program_fish_uTexture = program_fish.getUniformLocation( "uTexture" );
	program_fish_uMatrix = program_fish.getUniformLocation( "uMatrix" );
	program_fish_uPhaseFreqAmp = program_fish.getUniformLocation( "uPhaseFreqAmp" );
	////////////////////////////////

	////////////////////////////////
	// Textures and FrameBuffers
	fishTexture = new Texture2D( arguments.fishTexture );
	backgroundTexture = new Texture2D( arguments.backgroundImageFile );
	backgroundFrameBuffer = new FrameBuffer2D( backgroundTexture->getWidth(), backgroundTexture->getHeight(), GL_RGBA );
	waterFrameBufferDst = new FrameBuffer2D( backgroundTexture->getWidth()/arguments.waterResolutionDivider, backgroundTexture->getHeight()/arguments.waterResolutionDivider, GL_RGBA );
	waterFrameBufferSrc = new FrameBuffer2D( backgroundTexture->getWidth()/arguments.waterResolutionDivider, backgroundTexture->getHeight()/arguments.waterResolutionDivider, GL_RGBA );
	////////////////////////////////

	////////////////////////////////
	// Initialize fish
	for( unsigned int i = 0; i < arguments.numberOfFish; i++ )
	{
		Fish f;
		f.position[0] = randf() * 2.0f - 1.0f;
		f.position[1] = randf() * 2.0f - 1.0f;
		f.rotation = randf() * 2.0f * PI;
		f.scale = randf() * 0.02f + 0.08f;
		f.agility = randf() * 0.005f + 0.001f;
		f.minSpeed = randf() * 0.001f + 0.0005f;
		f.maxSpeed = randf() * 0.01f + 0.004f;
		f.rotationMaxSpeed = randf() * 0.4f + 0.1f;
		f.sensitivityDistance = randf() * 0.4f + 0.1f;
		fish.push_back( f );
	}
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
#ifdef GLESPOND_POINTIR
				case SDLK_SPACE:
					calibrate();
					break;
#endif
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

		waterFrameBufferSrc->bind();
		for( const auto & t : touches )
		{
			render_waterModulator( t.second.point, 0.03f );
		}

		waterFrameBufferDst->bind();
		render_water( waterFrameBufferSrc->getTexture(), waterFrameBufferSrc->getTexture()->getWidth(), waterFrameBufferSrc->getTexture()->getHeight() );

		backgroundFrameBuffer->bind();
		render_copy( backgroundTexture );
		update_fish( fish, touches );
		render_fish( fish );

		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		glViewport( 0, 0, w, h );
		render_waterDrawer( waterFrameBufferDst->getTexture(), backgroundFrameBuffer->getTexture() );

		std::swap( waterFrameBufferSrc, waterFrameBufferDst );

		SDL_GL_SwapWindow( window );
	}

	SDL_Quit();

	return 0;
}
