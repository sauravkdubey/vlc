/*****************************************************************************
 * misc.c
 *****************************************************************************
 * Copyright (C) 2007-2018 the VideoLAN team
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <errno.h>

#include <vlc_common.h>
#include <vlc_fs.h>

#include "../vlc.h"
#include "../libs.h"

static int vlclua_io_file_read_line( lua_State *L, FILE* p_file )
{
    char* psz_line = NULL;
    size_t i_buffer;
    ssize_t i_len = getline( &psz_line, &i_buffer, p_file );
    if ( i_len == -1 )
        return 0;
    if( psz_line[i_len - 1] == '\n' )
        psz_line[--i_len] = 0;
    lua_pushstring( L, psz_line );
    free( psz_line );
    return 1;
}

static int vlclua_io_file_read_number( lua_State *L, FILE* p_file )
{
    lua_Number num;
    if ( fscanf( p_file, LUA_NUMBER_SCAN, &num ) != 1 )
        return 0;
    lua_pushnumber( L, num );
    return 1;
}

static int vlclua_io_file_read_chars( lua_State *L, size_t i_len, FILE* p_file )
{
    size_t i_toread = LUAL_BUFFERSIZE;
    size_t i_read;
    luaL_Buffer b;
    luaL_buffinit( L, &b );
    do {
        char *p = luaL_prepbuffer(&b);
        if (i_toread > i_len)
            i_toread = i_len;
        i_read = fread(p, sizeof(char), i_toread, p_file);
        luaL_addsize(&b, i_read);
        i_len -= i_read;
    } while (i_len > 0 && i_read == i_toread);
    luaL_pushresult(&b);
    return (i_len == 0 || lua_objlen(L, -1) > 0);
}

static int vlclua_io_file_read( lua_State *L )
{
    FILE **pp_file = (FILE**)luaL_checkudata( L, 1, "io_file" );
    if( lua_type( L, 2 ) == LUA_TNUMBER )
    {
        return vlclua_io_file_read_chars( L, (size_t)lua_tointeger( L, 2 ),
                                          *pp_file );
    }
    const char* psz_mode = luaL_optstring( L, 2, "*l" );
    if ( *psz_mode != '*' )
        return luaL_error( L, "Invalid file:read() format: %s", psz_mode );
    switch ( psz_mode[1] )
    {
        case 'l':
            return vlclua_io_file_read_line( L, *pp_file );
        case 'n':
            return vlclua_io_file_read_number( L, *pp_file );
        case 'a':
            return vlclua_io_file_read_chars( L, SIZE_MAX, *pp_file );
        default:
            break;
    }
    return luaL_error( L, "Invalid file:read() format: %s", psz_mode );
}

static int vlclua_io_file_write( lua_State *L )
{
    FILE **pp_file = (FILE**)luaL_checkudata( L, 1, "io_file" );
    int i_nb_args = lua_gettop( L );
    bool b_success = true;
    for ( int i = 2; i <= i_nb_args; ++i )
    {
        bool i_res;
        if ( lua_type( L, i ) == LUA_TNUMBER )
            i_res = fprintf(*pp_file, LUA_NUMBER_FMT, lua_tonumber( L, i ) ) > 0;
        else
        {
            size_t i_len;
            const char* psz_value = luaL_checklstring( L, i, &i_len );
            i_res = fwrite(psz_value, sizeof(*psz_value), i_len, *pp_file ) > 0;
        }
        b_success = b_success && i_res;
    }
    lua_pushboolean( L, b_success );
    return 1;
}

static int vlclua_io_file_seek( lua_State *L )
{
    FILE **pp_file = (FILE**)luaL_checkudata( L, 1, "io_file" );
    const char* psz_mode = luaL_optstring( L, 2, NULL );
    if ( psz_mode != NULL )
    {
        long i_offset = luaL_optlong( L, 3, 0 );
        int i_mode;
        if ( !strcmp( psz_mode, "set" ) )
            i_mode = SEEK_SET;
        else if ( !strcmp( psz_mode, "end" ) )
            i_mode = SEEK_END;
        else
            i_mode = SEEK_CUR;
        if( fseek( *pp_file, i_offset, i_mode ) != 0 )
            return luaL_error( L, "Failed to seek" );
    }
    lua_pushinteger( L, ftell( *pp_file ) );
    return 1;
}

static int vlclua_io_file_flush( lua_State *L )
{
    FILE **pp_file = (FILE**)luaL_checkudata( L, 1, "io_file" );
    fflush( *pp_file );
    return 0;
}

static int vlclua_io_file_close( lua_State *L )
{
    FILE **pp_file = (FILE**)luaL_checkudata( L, 1, "io_file" );
    fclose( *pp_file );
    return 0;
}

static const luaL_Reg vlclua_io_file_reg[] = {
    { "read", vlclua_io_file_read },
    { "write", vlclua_io_file_write },
    { "seek", vlclua_io_file_seek },
    { "flush", vlclua_io_file_flush },
    { NULL, NULL }
};

static int vlclua_io_open( lua_State *L )
{
    if( lua_gettop( L ) < 1 )
        return luaL_error( L, "Usage: vlc.io.open(file_path [, mode])" );
    const char* psz_path = luaL_checkstring( L, 1 );
    const char* psz_mode = luaL_optstring( L, 2, "r" );
    FILE *p_f =  vlc_fopen( psz_path, psz_mode );
    if ( p_f == NULL )
        return 0;

    FILE** pp_f = lua_newuserdata( L, sizeof( p_f ) );
    *pp_f = p_f;

    if( luaL_newmetatable( L, "io_file" ) )
    {
        lua_newtable( L );
        luaL_register( L, NULL, vlclua_io_file_reg );
        lua_setfield( L, -2, "__index" );
        lua_pushcfunction( L, vlclua_io_file_close );
        lua_setfield( L, -2, "__gc" );
    }
    lua_setmetatable( L, -2 );
    return 1;
}

static int vlclua_mkdir( lua_State *L )
{
    if( lua_gettop( L ) < 2 ) return vlclua_error( L );

    const char* psz_dir = luaL_checkstring( L, 1 );
    const char* psz_mode = luaL_checkstring( L, 2 );
    if ( !psz_dir || !psz_mode )
        return vlclua_error( L );
    int i_res = vlc_mkdir( psz_dir, strtoul( psz_mode, NULL, 0 ) );
    lua_pushboolean( L, i_res == 0 || errno == EEXIST );
    return 1;
}

static const luaL_Reg vlclua_io_reg[] = {
    { "mkdir", vlclua_mkdir },
    { "open", vlclua_io_open },

    { NULL, NULL }
};

void luaopen_vlcio( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_io_reg );
    lua_setfield( L, -2, "io" );
}
