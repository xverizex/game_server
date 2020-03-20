/*
 * game_server - сервер для игры broken_heads
 *
 * Copyright (C) 2020 Naidolinsky Dmitry <naidv88@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY of FITNESS for A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * -------------------------------------------------------------------/
 */
#include "config.h"
#include "vars.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

struct conf conf;

#define DEFAULT_LENGTH_LINE      255

static FILE *fp;
static char *line;

static char *get_line ( ) {
	if ( !line ) line = calloc ( DEFAULT_LENGTH_LINE, 1 );
	else memset ( line, 0, DEFAULT_LENGTH_LINE );
	char *res = fgets ( line, DEFAULT_LENGTH_LINE - 1, fp );
	return res;
}

static void check_config_file ( ) {
	if ( access ( DEFAULT_CONFIG_FILE, F_OK ) ) {
		perror ( "access" );
		exit ( EXIT_FAILURE );
	}
}
static void open_config_file ( ) {
	fp = fopen ( DEFAULT_CONFIG_FILE, "r" );
	if ( !fp ) {
		perror ( "fopen config file" );
		exit ( EXIT_FAILURE );
	}
}

#define OPT_LISTEN         0
#define OPT_PORT           1
#define OPT_DB_USER        2
#define OPT_DB_HOST        3
#define OPT_DB_PASSWD      4
#define OPT_DB_DB          5
#define OPT_ADMIN_USER     6
#define OPT_ADMIN_PASSWD   7
#define OPT_GAMER_GOLD     8

static int get_opt ( const char *opt ) {
	if ( !strncmp ( opt, "listen", strlen ( opt ) ) ) return OPT_LISTEN;
	if ( !strncmp ( opt, "port", strlen ( opt ) ) ) return OPT_PORT;
	if ( !strncmp ( opt, "db_user", strlen ( opt ) ) ) return OPT_DB_USER;
	if ( !strncmp ( opt, "db_host", strlen ( opt ) ) ) return OPT_DB_HOST;
	if ( !strncmp ( opt, "db_passwd", strlen ( opt ) ) ) return OPT_DB_PASSWD;
	if ( !strncmp ( opt, "db_db", strlen ( opt ) ) ) return OPT_DB_DB;
	if ( !strncmp ( opt, "admin_user", strlen ( opt ) ) ) return OPT_ADMIN_USER;
	if ( !strncmp ( opt, "admin_passwd", strlen ( opt ) ) ) return OPT_ADMIN_PASSWD;
	if ( !strncmp ( opt, "gamer_gold", strlen ( opt ) ) ) return OPT_GAMER_GOLD;

}

#define CHECK_TRUE        0
#define CHECK_FALSE      -1

static int check_val_on_number ( const char *val ) {
	int length = strlen ( val );
	for ( int i = 0; i < length; i++ ) {
		if ( *val >= '0' && *val <= '9' ) continue;
		return CHECK_FALSE;
	}
	return CHECK_TRUE;
}

static void parse_line ( const char *s ) {
	char opt[255] = { 0 };
	char val[255] = { 0 };

	int space = 0;
	int pos = 0;
	/* получить opt */
	for ( int i = 0; *s != '\n' && *s != 0 && *s != '=' && pos < DEFAULT_LENGTH_LINE; s++, i++, pos++ ) {
		if ( !space ) {
			opt[i] = *s;
			continue;
		}
		if ( *s == ' ' ) space = 1;
	}
	s++;
	while ( *s == ' ' ) s++;
	/* получить val */
	for ( int i = 0; *s != '\n' && *s != 0 && pos < DEFAULT_LENGTH_LINE; s++, i++, pos++ ) {
		if ( *s == ' ' ) break;
		val[i] = *s;
	}

	int sopt = get_opt ( opt );

	switch ( sopt ) {
		case OPT_LISTEN:
			{
				int ret = check_val_on_number ( val );
				if ( ret == CHECK_FALSE ) {
					fprintf ( stderr, "error: %s=%s will be number.\n", opt, val );
					exit ( EXIT_FAILURE );
				}
				conf.listen = atoi ( val );
				return;
			}
			break;
		case OPT_PORT:
			{
				int ret = check_val_on_number ( val );
				if ( ret == CHECK_FALSE ) {
					fprintf ( stderr, "error: %s=%s will be number.\n", opt, val );
					exit ( EXIT_FAILURE );
				}
				conf.port = atoi ( val );
				return;
			}
			break;
		case OPT_DB_USER:
			{
				memset ( &conf.user[0], 0, CONF_SIZE_ARRAY );
				strncpy ( &conf.user[0], val, strlen ( val ) );
				return;
			}
			break;
		case OPT_DB_HOST:
			{
				memset ( &conf.host[0], 0, CONF_SIZE_ARRAY );
				strncpy ( &conf.host[0], val, strlen ( val ) );
				return;
			}
			break;
		case OPT_DB_PASSWD:
			{
				memset ( &conf.passwd[0], 0, CONF_SIZE_ARRAY );
				strncpy ( &conf.passwd[0], val, strlen ( val ) );
				return;
			}
			break;
		case OPT_DB_DB:
			{
				memset ( &conf.db[0], 0, CONF_SIZE_ARRAY );
				strncpy ( &conf.db[0], val, strlen ( val ) );
				return;
			}
			break;
		case OPT_ADMIN_USER:
			{
				memset ( &conf.admin_user[0], 0, CONF_SIZE_ARRAY );
				strncpy ( &conf.admin_user[0], val, strlen ( val ) );
				return;
			}
			break;
		case OPT_ADMIN_PASSWD:
			{
				memset ( &conf.admin_passwd[0], 0, CONF_SIZE_ARRAY );
				strncpy ( &conf.admin_passwd[0], val, strlen ( val ) );
				return;
			}
			break;
		case OPT_GAMER_GOLD:
			{
				int ret = check_val_on_number ( val );
				if ( ret == CHECK_FALSE ) {
					fprintf ( stderr, "error: %s=%s will be number.\n", opt, val );
					exit ( EXIT_FAILURE );
				}
				conf.gamer_gold = atoi ( val );
				return;
			}
			break;
	}
}

void parse_conf ( ) {
	check_config_file ( );
	open_config_file ( );

	char *s;
	while ( s = get_line ( ) ) {
		if ( s[0] == '#' ) continue;
		parse_line ( s );
	}

	fclose ( fp );
}
