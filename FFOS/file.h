/**
File, file mapping, pipe, console I/O.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#ifdef FF_WIN
#include <FFOS/win/file.h>
#include <FFOS/win/fmap.h>
#else
#include <FFOS/unix/file.h>
#include <FFOS/unix/fmap.h>
#endif
