/*
    constants.h : espctag
    v0.4 - 2012-08-29
    
    Copyright (C) 2011,2012 Jérôme SONRIER <jsid@emor3j.fr.eu.org>

    This file is part of espctag.

    espctag is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    espctag is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with espctag.  If not, see <http://www.gnu.org/licenses/>.
*/

#define MAX_FILENAME_LENGTH 4096

/* Tags index */
#define I_SONG_TITLE  0
#define I_GAME_TITLE  1
#define I_DUMPER_NAME 2
#define I_COMMENTS    3
#define I_DUMP_DATE   4
#define I_LENGTH      5
#define I_FADE_LENGTH 6
#define I_ARTIST      7
#define I_CHANNELS    8
#define I_EMULATOR    9

/* Return codes */
#define SUCCESS          0
#define E_WRONG_ARG   -100
#define E_OPEN_FILE   -200
#define E_RENAME_FILE -201
#define E_CREATE_DIR  -202
#define E_OPEN_DIR    -203
#define E_CH_DIR      -204
#define E_DEL_DIR     -205
#define E_DEL_FILE    -206
#define E_FORK        -300
#define E_PACK_RSN    -400
#define E_UNPACK_RSN  -401
