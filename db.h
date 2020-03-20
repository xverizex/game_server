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
#ifndef __DB_H__
#define __DB_H__
void db_init ( );
void db_connect ( );
void db_clean_online ( );
void db_offline_user ( const int fd );
int db_registration ( const char *dt, const int fd );
int db_register_admin ( const char *dt );
void db_admin ( );
int buy_items ( const char *dt, const int fd );
int select_persona ( const char *dt, const int fd );
int db_register_with_login ( const char *dt, const int fd );
int db_get_characteristics ( const char *dt, const int fd );
int db_write_persona_new ( const char *dt, const int fd );
int db_auth_with_login ( const char *dt, const int fd );
int db_get_main_menu ( const int fd );
int db_get_money_and_shop ( const int fd );
int db_get_characteristics_menu_fight ( const int fd );
#endif
