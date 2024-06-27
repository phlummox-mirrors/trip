/* Copyright 2020-2024 Philip Kaludercic
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>

#include "trip.h"

#ifndef DEF
#define DEF(ret, name, params, args, fail, ...)				\
     ret name params {							\
          typedef ret (*real) params;					\
          int errv[] = { __VA_ARGS__ };					\
          return ____trip_should_fail(#name, errv, LENGTH(errv))	\
               ? fail							\
               : (((real) dlsym(RTLD_NEXT, #name)) args);		\
     }
#endif

#define E(e) e
