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
#ifndef __CONFIG_H__
#define __CONFIG_H__
#define CONF_SIZE_ARRAY       255
struct conf {
	/* максимальное число клиентов для подключения */
	unsigned int listen;
	unsigned short port;
	char user[CONF_SIZE_ARRAY];
	char host[CONF_SIZE_ARRAY];
	char passwd[CONF_SIZE_ARRAY];
	char db[CONF_SIZE_ARRAY];
	char admin_user[CONF_SIZE_ARRAY];
	char admin_passwd[CONF_SIZE_ARRAY];
	unsigned int gamer_gold;
};

void parse_conf ( );
#endif
