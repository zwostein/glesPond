# Locate VorbisFile library
# This module defines
# VorbisFile_FOUND       - system has VorbisFile
# VorbisFile_INCLUDE_DIR - vorbisfile.h directory
# VorbisFile_LIBRARY     - library to link
#
# $VORBISDIR is an environment variable used
# for finding Vorbis.

find_path(VorbisFile_INCLUDE_DIR vorbis/vorbisfile.h
  HINTS
    ENV VORBISDIR
  PATH_SUFFIXES include
  PATHS
  /opt/local
)

find_library(VorbisFile_LIBRARY
  NAMES vorbisfile
  HINTS
    ENV VORBISDIR
  PATH_SUFFIXES lib64 lib libs64 libs libs/Win32 libs/Win64
  PATHS
  /opt/local
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(VorbisFile  DEFAULT_MSG VorbisFile_LIBRARY OPENAL_INCLUDE_DIR)

mark_as_advanced(VorbisFile_LIBRARY VorbisFile_INCLUDE_DIR)
