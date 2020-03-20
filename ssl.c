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
#include <string.h>
#include <stdio.h>
#include <openssl/sha.h>

#define SSL_SIZE_SHA256        SHA256_DIGEST_LENGTH

void get_sha256 ( const char *dt, char out[65] ) {
	unsigned char hash[SHA256_DIGEST_LENGTH];
	unsigned int len = strlen ( dt );

	SHA256_CTX sha256;
	SHA256_Init ( &sha256 );
	SHA256_Update ( &sha256, dt, len );
	SHA256_Final ( hash, &sha256 );

	for ( int i = 0; i < SHA256_DIGEST_LENGTH; i++ ) {
		sprintf ( out + ( i * 2 ), "%02x", hash[i] );
	}
	out[64] = 0;
}
