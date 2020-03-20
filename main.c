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
#include <stdio.h>
#include "server.h"
#include "config.h"
#include "db.h"

int main ( int argc, char **argv ) {
	parse_conf ( );
	db_init ( );
	db_connect ( );
	db_clean_online ( );
	db_admin ( );
	create_thread ( );
	configure_socket ( );

	while ( 1 ) {
		wait_client ( );
	}
}
