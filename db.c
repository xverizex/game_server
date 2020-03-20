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
#include <mysql/mysql.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "config.h"
#include "events.h"
#include "ssl.h"

extern struct conf conf;

MYSQL db;
MYSQL dbb;

#define DB_SIZE_QUERY         1024 
#define DB_PARAM_SIZE          256 

void db_init ( ) {
	mysql_init ( &db );
	mysql_init ( &dbb );
}

void db_connect ( ) { 
	mysql_real_connect ( &db,
		conf.host,
		conf.user,
		conf.passwd,
		conf.db,
		0,
		NULL,
		0);
	mysql_real_connect ( &dbb,
		conf.host,
		conf.user,
		conf.passwd,
		conf.db,
		0,
		NULL,
		0);
}

void db_admin ( ) {
	char query[DB_SIZE_QUERY];
	snprintf ( query, DB_SIZE_QUERY, "delete from admin;" );
	mysql_query ( &db, query );

	char hash[65];
	get_sha256 ( conf.admin_passwd, hash );

	/* записать новый пароль */
	snprintf ( query, DB_SIZE_QUERY,
			"insert into admin ( login, passwd ) "
			"values ( '%s', '%s' );",
			conf.admin_user,
			hash
		 );
	mysql_query ( &db, query );
}

int db_register_admin ( const char *dt ) {
}

static void check_param ( char *s ) {
	for ( ; *s != 0; s++ ) {
		switch ( *s ) {
			case '\'':
				*s = ',';
				break;
			case '`':
				*s = ',';
				break;
		}
	}
}

void db_clean_online ( ) {
	const char *query = "delete from online;";
	mysql_query ( &db, query );
	const char *query_clean_online = "update users set online = '0';";
	mysql_query ( &db, query_clean_online );
}

void db_offline_user ( const int fd ) {
	char query[DB_SIZE_QUERY];
	/* получить id_user */
	snprintf ( query, DB_SIZE_QUERY, "select id_user from online where fd = '%d';", fd );
	mysql_query ( &db, query );
	MYSQL_RES *store = mysql_store_result ( &db );
	long unsigned int length = mysql_num_rows ( store );
	if ( length == 0 ) return;
	MYSQL_ROW row = mysql_fetch_row ( store );
	int id_user = atoi ( row[0] );
	mysql_free_result ( store );


	snprintf ( query, DB_SIZE_QUERY, "delete from online where fd = '%d';", fd );
	mysql_query ( &db, query );

	snprintf ( query, DB_SIZE_QUERY, "update users set online = '0' where id = '%d';", id_user );
	mysql_query ( &db, query );
}

static int check_num ( const char *n ) {
	int length = strlen ( n );
	for ( int i = 0; i < length; i++ ) {
		if ( *n >= '0' && *n <= '9' ) continue;
		return EVENT_EVENT_FALSE;
	}
	return EVENT_EVENT_TRUE;
}


int read_param ( char *param, const char **s ) {
	while ( *(*s) == '\n' ) (*s)++;
	int i = 0;
	for ( ; i < 126 && *(*s) != ';' && *(*s) != 0 && *(*s) != '\n'; i++, (*s)++ ) {
		param[i] = *(*s);
	}
	param[i] = 0;
	if ( i == 0 ) return EVENT_EVENT_FALSE;
	return EVENT_EVENT_TRUE;
}

int db_get_characteristics ( const char *dt, const int fd ) {
	/* проверить, есть ли онлайн такой пользователь */
#define QUERY_SIZE          4096
	char query[QUERY_SIZE];
	snprintf ( query, QUERY_SIZE, "select id from online where fd = '%d';", fd );
	mysql_query ( &db, query );
	MYSQL_RES *store = mysql_store_result ( &db );
	long unsigned int length = mysql_num_rows ( store );
	mysql_free_result ( store );
	if ( length == 0 ) return EVENT_EVENT_FALSE;

	/* проверить входящие данные */
#define PARAMS_MONSTERS             1
#define TYPE_OF_MONSTER             0
	const char *s = dt;
	char monster[PARAMS_MONSTERS][QUERY_SIZE];
	for ( int i = 0; i < PARAMS_MONSTERS; i++ ) {
		if ( read_param ( monster[i], &s ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;
		if ( *s++ != ';' ) return EVENT_EVENT_FALSE;
	}
	if ( strlen ( monster[TYPE_OF_MONSTER] ) > 1 ) return EVENT_EVENT_FALSE;

	if ( monster[TYPE_OF_MONSTER][0] == '0' ) return EVENT_EVENT_FALSE; // если персонаж общий, то ошибка

	/* узнать есть ли такой монстр */
	snprintf ( query, QUERY_SIZE, "select id from personas where persona = '%c';", monster[TYPE_OF_MONSTER][0] );
	mysql_query ( &db, query );
	store = mysql_store_result ( &db );
	length = mysql_num_rows ( store );
	mysql_free_result ( store );
	if ( length == 0 ) return EVENT_EVENT_FALSE;

	/* получить характеристики персонажа */
	snprintf ( query, QUERY_SIZE, "select health, attack, accuracy, agility from initial_characteristics where monster = '%c';",
			monster[TYPE_OF_MONSTER][0]
		 );
	mysql_query ( &db, query );
	store = mysql_store_result ( &db );
	length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }
	MYSQL_ROW row = mysql_fetch_row ( store );
	int health = 0;
	int attack = 0;
	int accuracy = 0;
	int agility = 0;
#define PARAM_HEALTH         0
#define PARAM_ATTACK         1
#define PARAM_ACCURACY       2
#define PARAM_AGILITY        3
	if ( row[PARAM_HEALTH] && row[PARAM_ATTACK] && row[PARAM_ACCURACY] && row[PARAM_AGILITY] ) {
		health = atoi ( row[PARAM_HEALTH] );
		attack = atoi ( row[PARAM_ATTACK] );
		accuracy = atoi ( row[PARAM_ACCURACY] );
		agility = atoi ( row[PARAM_AGILITY] );
	}
	mysql_free_result ( store );
	snprintf ( query, QUERY_SIZE, "1;%d;%d;%d;%d;", health, attack, accuracy, agility );
	write ( fd, query, strlen ( query ) );
	return EVENT_EVENT_TRUE;
}

static int check_online ( const int fd ) {
#define QUERY_SIZE          4096
	char query[QUERY_SIZE];
	snprintf ( query, QUERY_SIZE, "select id_user from online where fd = '%d';", fd );
	mysql_query ( &db, query );
	MYSQL_RES *store = mysql_store_result ( &db );
	long unsigned int length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }
	return EVENT_EVENT_TRUE;
}
static int check_online_id_user ( const int id_user ) {
#define QUERY_SIZE          4096
	char query[QUERY_SIZE];
	snprintf ( query, QUERY_SIZE, "select id from online where id_user = '%d';", id_user );
	mysql_query ( &db, query );
	MYSQL_RES *store = mysql_store_result ( &db );
	long unsigned int length = mysql_num_rows ( store );
	mysql_free_result ( store );
	if ( length == 0 ) return EVENT_EVENT_TRUE;
	return EVENT_EVENT_FALSE;
}

static int check_online_and_get_id_user ( const int fd ) {
#define QUERY_SIZE          4096
	char query[QUERY_SIZE];
	snprintf ( query, QUERY_SIZE, "select id_user from online where fd = '%d';", fd );
	mysql_query ( &db, query );
	MYSQL_RES *store = mysql_store_result ( &db );
	long unsigned int length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }
	MYSQL_ROW row = mysql_fetch_row ( store );
#define ID_USER               0
	int id_user = atoi ( row[ID_USER] );
	mysql_free_result ( store );
	return id_user;
}
static int check_on_start_and_get_id_user ( const char *nick ) {
#define QUERY_SIZE          4096
	char query[QUERY_SIZE];
	snprintf ( query, QUERY_SIZE, "select id from users where nick = '%s';", nick );
	mysql_query ( &db, query );
	MYSQL_RES *store = mysql_store_result ( &db );
	long unsigned int length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }
	MYSQL_ROW row = mysql_fetch_row ( store );
#define ID_USER               0
	int id_user = atoi ( row[ID_USER] );
	mysql_free_result ( store );
	return id_user;
}

static int check_persona ( const int fd ) {
	printf ( "check_persona\n" );
	int id_user = -1;
	if ( ( id_user = check_online_and_get_id_user ( fd ) ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;

	/* проверить, есть ли уже есть ли созданный персонаж, если ещё не создан, то направить на страницу создания */
#define QUERY_SIZE          4096
	char query[QUERY_SIZE];
	snprintf ( query, QUERY_SIZE, "select id from gamers_personas where id_user = '%d';", id_user );
	mysql_query ( &db, query );
	MYSQL_RES *store = mysql_store_result ( &db );
	long unsigned int length = mysql_num_rows ( store );
	printf ( "persona length: %d\n", length );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_USER_NOT_CREATED; }

	mysql_free_result ( store );
	return EVENT_USER_CREATED;

}

static int check_persona_on_start ( const char *login, int *ptr_id_user ) {
	int id_user = -1;
	if ( ( id_user = check_on_start_and_get_id_user ( login ) ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;
	*ptr_id_user = id_user;

	/* проверить, есть ли уже есть ли созданный персонаж, если ещё не создан, то направить на страницу создания */
#define QUERY_SIZE          4096
	char query[QUERY_SIZE];
	snprintf ( query, QUERY_SIZE, "select id from gamers_personas where id_user = '%d';", id_user );
	mysql_query ( &db, query );
	MYSQL_RES *store = mysql_store_result ( &db );
	long unsigned int length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_USER_NOT_CREATED; }

	mysql_free_result ( store );
	return EVENT_USER_CREATED;

}

static int check_login_and_password ( const char *dt ) {
	const char *s = dt;
#define PARAMS_AUTHORIZATION        2
#define LOGIN                       0
#define PASSWORD                    1
#define QUERY_SIZE               4096
	char reg_data[PARAMS_AUTHORIZATION][QUERY_SIZE];
	for ( int i = 0; i < PARAMS_AUTHORIZATION; i++ ) {
		if ( read_param ( reg_data[i], &s ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;
		if ( *s++ != ';' ) return EVENT_EVENT_FALSE;
	}
	/* проверить длину пароля, пароль должен быть не менее 6 символов. */
	if ( strlen ( reg_data[PASSWORD] ) < 6 ) return EVENT_EVENT_FALSE;
	/* проверить, есть ли такой логин */
	char query[QUERY_SIZE];
	snprintf ( query, QUERY_SIZE, "select password from users where nick = '%s';", reg_data[LOGIN] );
	mysql_query ( &db, query );
	MYSQL_RES *store = mysql_store_result ( &db );

	long unsigned int length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }
#define RES_PASSWORD          0
	MYSQL_ROW row = mysql_fetch_row ( store );

	/* зарегестрировать нового пользователя */
	char hash[65];
	get_sha256 ( reg_data[PASSWORD], hash );

	int len = strlen ( row[RES_PASSWORD] );

	if ( !strncmp ( row[RES_PASSWORD], hash, len ) ) { mysql_free_result ( store ); return EVENT_EVENT_TRUE; }

	mysql_free_result ( store );
	return EVENT_EVENT_FALSE;
}

static int get_type_of_monster ( const int id_user ) {
#define QUERY_SIZE               4096
	char query[QUERY_SIZE];
	snprintf ( query, QUERY_SIZE, "select itype from gamers_personas where id_user = '%d';", id_user );
	mysql_query ( &db, query );

	MYSQL_RES *store = mysql_store_result ( &db );
	long unsigned int length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }
#define ROW_TYPE              0
	MYSQL_ROW row = mysql_fetch_row ( store );
	int type = atoi ( row[ROW_TYPE] );
	mysql_free_result ( store );
	return type;
}

static void write_to_online_fd ( const int id_user, const int fd ) {
#define QUERY_SIZE               4096
	char query[QUERY_SIZE];
	snprintf ( query, QUERY_SIZE, "insert into online ( id_user, fd ) values ( %d, %d );", id_user, fd );
	mysql_query ( &db, query );
	snprintf ( query, QUERY_SIZE, "update users set online = '1' where id = '%d';", id_user );
	mysql_query ( &db, query );
}
static void write_to_online_fd_oauth ( const int id_user, const int fd, const char *user_id ) {
#define QUERY_SIZE               4096
	char query[QUERY_SIZE];
	snprintf ( query, QUERY_SIZE, "insert into online ( id_user, user_id, fd ) values ( %d, '%s', %d );", id_user, user_id, fd );
	mysql_query ( &db, query );
	snprintf ( query, QUERY_SIZE, "update users set online = '1' where id = '%d';", id_user );
	mysql_query ( &db, query );
}

int db_get_characteristics_menu_fight ( const int fd ) {
	printf ( "db_get_characteristics_menu_fight\n" );
	int id_user = check_online_and_get_id_user ( fd );
	if ( id_user == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;

	/* получить характеристики своего персонажа */
#define QUERY_SIZE              4096
#define MENU_FIGHT_STORE        255
#define MENU_FIGHT_PARAMS       7
#define MENU_FIGHT_ROW_LEVEL    0
#define MENU_FIGHT_ROW_HEALTH   1
#define MENU_FIGHT_ROW_ATTACK   2
#define MENU_FIGHT_ROW_ACCURACY 3
#define MENU_FIGHT_ROW_AGILITY  4
#define MENU_FIGHT_ROW_NAME     5
#define MENU_FIGHT_ROW_TYPE     6
	char query[QUERY_SIZE];
	char pers[MENU_FIGHT_PARAMS][MENU_FIGHT_STORE];
	/* сначала получить характеристики персонажа */
	snprintf ( query, QUERY_SIZE, "select ilevel, health, attack, accuracy, agility, name, itype from gamers_personas where id_user = '%d';", id_user );
	mysql_query ( &db, query );
	MYSQL_RES *store = mysql_store_result ( &db );
	long unsigned int length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }
	MYSQL_ROW row = mysql_fetch_row ( store );
	printf ( "копирования характеристик для игрока\n" );
	strncpy ( pers[MENU_FIGHT_ROW_LEVEL], row[0], strlen ( row[0] ) + 1 );
	strncpy ( pers[MENU_FIGHT_ROW_HEALTH], row[1], strlen ( row[1] ) + 1 );
	strncpy ( pers[MENU_FIGHT_ROW_ATTACK], row[2], strlen ( row[2] ) + 1 );
	strncpy ( pers[MENU_FIGHT_ROW_ACCURACY], row[3], strlen ( row[3] ) + 1 );
	strncpy ( pers[MENU_FIGHT_ROW_AGILITY], row[4], strlen ( row[4] ) + 1 );
	strncpy ( pers[MENU_FIGHT_ROW_NAME], row[5], strlen ( row[5] ) + 1 );
	strncpy ( pers[MENU_FIGHT_ROW_TYPE], row[6], strlen ( row[6] ) + 1 );
	printf ( "завершение копирования характеристик для игрока\n" );
	mysql_free_result ( store );

	/* ответ для игрока */
#define MENU_FIGHT_ANSWER_SIZE      8192
	char answer[MENU_FIGHT_ANSWER_SIZE];
	char *s = &answer[0];
	int n;
	int total = MENU_FIGHT_ANSWER_SIZE;
	snprintf ( s, total, "1;%n", &n );
	s += n;
	total -= n;

	/* записать параметры игрока */
	snprintf ( s, total, "%s;%s;%s;%s;%s;%s;%n",
		pers[MENU_FIGHT_ROW_LEVEL],
		pers[MENU_FIGHT_ROW_HEALTH],
		pers[MENU_FIGHT_ROW_ATTACK],
		pers[MENU_FIGHT_ROW_ACCURACY],
		pers[MENU_FIGHT_ROW_AGILITY],
		pers[MENU_FIGHT_ROW_NAME],
		&n
		);
	s += n;
	total -= n;

	/* получить онлайн пользователей */
	int limit = 20;
	printf ( "limit для поиска online пользователей\n" );
	snprintf ( query, QUERY_SIZE, "select id_user from online limit %d;", limit );
	mysql_query ( &db, query );
	store = mysql_store_result ( &db );
	length = mysql_num_rows ( store );
	limit = limit - length;
	/* указать сколько пользователей онлайн */
	snprintf ( s, total, "%ld;%n", length, &n );
	s += n;
	total -= n;


	for ( int i = 0; i < length; i++ ) {
		row = mysql_fetch_row ( store );
		int idd_user = atoi ( row[0] );
		if ( id_user == idd_user ) continue;
		printf ( "поиск противника онлайн\n" );
		snprintf ( query, QUERY_SIZE, "select name, ilevel, health, attack, accuracy, agility, id_user from gamers_personas where id_user = '%d';", idd_user );
		printf ( "!%s\n", query );
		mysql_query ( &dbb, query );
		printf ( "запрос успешен с id=%d\n", idd_user );
		MYSQL_RES *store_dbb = mysql_store_result ( &dbb );
		MYSQL_ROW row_dbb = mysql_fetch_row ( store_dbb );
		snprintf ( s, total, "%s;%s;%s;%s;%s;%s;%n",
			row_dbb[0],
			row_dbb[1],
			row_dbb[2],
			row_dbb[3],
			row_dbb[4],
			row_dbb[5],
			row_dbb[6],
			&n
			);
		mysql_free_result ( store_dbb );
		s += n;
		total -= n;
	}
	mysql_free_result ( store );

	printf ( "поиск тех, кто оффлайн\n" );
	snprintf ( query, QUERY_SIZE, "select id from users where online = '0' limit %d;", limit );
	mysql_query ( &db, query );
	store = mysql_store_result ( &db );
	length = mysql_num_rows ( store );
	printf ( "длина: %ld\n", length );
	for ( int i = 0; i < length; i++ ) {
		row = mysql_fetch_row ( store );
		int idd_user = atoi ( row[0] );
		if ( id_user == idd_user ) continue;
		printf ( "поиск противника оффлайн\n" );
		snprintf ( query, QUERY_SIZE, "select name, ilevel, health, attack, accuracy, agility, id_user from gamers_personas where id_user = '%d';", idd_user );
		mysql_query ( &dbb, query );
		MYSQL_RES *store_dbb = mysql_store_result ( &dbb );
		MYSQL_ROW row_dbb = mysql_fetch_row ( store_dbb );
		snprintf ( s, total, "%s;%s;%s;%s;%s;%s;%s;%n",
			row_dbb[0],
			row_dbb[1],
			row_dbb[2],
			row_dbb[3],
			row_dbb[4],
			row_dbb[5],
			row_dbb[6],
			&n
			);
		mysql_free_result ( store_dbb );
		s += n;
		total -= n;
	}
	*s = 0;

	mysql_free_result ( store );
	write ( fd, answer, strlen ( answer ) );
	printf ( "итог: %s\n", answer );
	return EVENT_EVENT_TRUE;
}

int db_get_money_and_shop ( const int fd ) {
	int id_user = check_online_and_get_id_user ( fd );
	if ( id_user == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;

	/* получить золото и рубины */
#define QUERY_SIZE              4096
	char query[QUERY_SIZE];
	snprintf ( query, QUERY_SIZE, "select gold, rubies from gamers_personas where id_user = '%d';", id_user );
	mysql_query ( &db, query );
	MYSQL_RES *store = mysql_store_result ( &db );
	long unsigned int length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }
	MYSQL_ROW row = mysql_fetch_row ( store );
#define SHOP_GOLD_MAX   2
#define SHOP_GOLD       0
#define SHOP_RUBIES     1
	char money[SHOP_GOLD_MAX][QUERY_SIZE];
	strncpy ( money[SHOP_GOLD], row[SHOP_GOLD], strlen ( row[SHOP_GOLD] ) + 1 );
	strncpy ( money[SHOP_RUBIES], row[SHOP_RUBIES], strlen ( row[SHOP_RUBIES] ) + 1 );
	mysql_free_result ( store );

	/* получить список оружия игрока 
	 * это нужно чтобы проверить, если такое оружие уже есть у игрока, то не передавать в магазин
	 */
	snprintf ( query, QUERY_SIZE, "select items from gamers_personas where id_user = '%d';", id_user );
	mysql_query ( &db, query );
	int level = 0;
	store = mysql_store_result ( &db );
	length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }
#define BUY_ITEM_SIZE        1024
	char items[BUY_ITEM_SIZE];
	if ( row[0] ) {
		strncpy ( &items[0], row[0], strlen ( row[0] ) + 1 );
	} else {
		items[0] = 0;
	}
	mysql_free_result ( store );

	struct items {
		int count;
		int i[512];
	} it;

	void parse_items ( ) {
		it.count = 0;
		int length = strlen ( items );
		if ( length == 0 ) return;
		char itm[5];
		char *s = &items[0];
		char *st = s;
		int sp = 0;
		for ( int i = 0; i < length; i++, s++ ) {
			if ( *s == ',' ) {
				sp = 1;
				int len = s - st;
				strncpy ( &itm[0], st, len );
				itm[len] = 0;
				it.i[it.count++] = atoi ( itm );
				st = s + 1;
			}
		}
		if ( sp == 1 ) {
			int len = s - st;
			strncpy ( &itm[0], st, len );
			itm[len] = 0;
			it.i[it.count++] = atoi ( itm );
		}
		if ( sp == 0 && length > 0 ) {
			it.count = 1;
			it.i[0] = atoi ( items );
		}
	}

	parse_items ( );

	/* получить всё доступное оружие 
	 * если появиться нормальный магазин, то надо будет сделать проверку на уровень доступности для игрока.
	 * 
	 */
	snprintf ( query, QUERY_SIZE, "select icon, gold, num from items;" );
	mysql_query ( &db, query );
	store = mysql_store_result ( &db );
	length = mysql_num_rows ( store );

	char *s = &query[0];
	int n;
	int total = QUERY_SIZE;
	snprintf ( query, QUERY_SIZE, "1;%s;%s;%n", money[SHOP_GOLD], money[SHOP_RUBIES], &n );
	s += n;
	total -= n;

#define ITEM_ITEM_ICON          0
#define ITEM_ITEM_GOLD          1
#define ITEM_ITEM_NUMBER        2
	/* дополнить запрос вещами */
	for ( int i = 0; i < length; i++ ) {
		row = mysql_fetch_row ( store );
		int ok = 1;
		int number = atoi ( row[ITEM_ITEM_NUMBER] );
		for ( int ii = 0; ii < it.count; ii++ ) {
			if ( it.i[ii] == number ) { ok = 0; break; }
		}
		if ( ok ) {
			snprintf ( s, total, "%s;%s;%n", row[ITEM_ITEM_ICON], row[ITEM_ITEM_GOLD], &n );
			s += n;
			total -= n;
		}
	}

	/* отправить ответ */
	write ( fd, query, strlen ( query ) );

	mysql_free_result ( store );

	return EVENT_EVENT_TRUE;
}

int db_auth_with_login ( const char *dt, const int fd ) {
	const char *s = dt;
#define PARAMS_AUTHORIZATION        2
#define LOGIN                       0
#define PASSWORD                    1
#define QUERY_SIZE               4096
	char reg_data[PARAMS_AUTHORIZATION][QUERY_SIZE];
	for ( int i = 0; i < PARAMS_AUTHORIZATION; i++ ) {
		if ( read_param ( reg_data[i], &s ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;
		if ( *s++ != ';' ) return EVENT_EVENT_FALSE;
	}

	/* проверить логин и пароль */
	if ( check_login_and_password ( dt ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;

	/* получить тип персонажа и передать его. это нужно чтобы клиент знал какого персонажа рисовать */
	

	int id_user = 0;
	int type;
	int ret;
	/* отправить на соответстующую страницу */
	switch ( check_persona_on_start ( reg_data[LOGIN], &id_user ) ) {
		case EVENT_EVENT_FALSE: return EVENT_EVENT_FALSE; break;
		case EVENT_USER_NOT_CREATED: 
					ret = check_online_id_user ( id_user );
					if ( ret == EVENT_EVENT_FALSE ) {
						write ( fd, "0", 2 );
						return EVENT_EVENT_DISCONNECT;
					}
					write_to_online_fd ( id_user, fd );
					write ( fd, "3", 2 ); 
					break;
		case EVENT_USER_CREATED: 
					ret = check_online_id_user ( id_user );
					if ( ret == EVENT_EVENT_FALSE ) {
						write ( fd, "0", 2 );
						return EVENT_EVENT_DISCONNECT;
					}
					write_to_online_fd ( id_user, fd );
					type = get_type_of_monster ( id_user );
					if ( type == EVENT_EVENT_FALSE ) {
						write ( fd, "2", 2 ); 
					} else {
						char data[10];
						snprintf ( data, 10, "2%d", type );
						write ( fd, data, strlen ( data ) );
					}
					break;
	}

	return EVENT_EVENT_TRUE;
}

int db_write_persona_new ( const char *dt, const int fd ) {
	printf ( "db_write_persona_new\n" );
	const char *s = dt;
#define QUERY_SIZE          4096
	char query[QUERY_SIZE];
	snprintf ( query, QUERY_SIZE, "select id_user from online where fd = '%d';", fd );
	mysql_query ( &db, query );
	MYSQL_RES *store = mysql_store_result ( &db );
	long unsigned int length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }
	MYSQL_ROW row = mysql_fetch_row ( store );
	int id_user = atoi ( row[0] );
	mysql_free_result ( store );

	char name[512];
	snprintf ( query, 512, "select name from users where id = '%d';", id_user );
	mysql_query ( &db, query );
	store = mysql_store_result ( &db );
	length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }
	row = mysql_fetch_row ( store );
	strncpy ( name, row[0], strlen ( row[0] ) + 1 );
	mysql_free_result ( store );

	/* проверить входящие данные */
#define PARAMS_MONSTERS             1
#define TYPE_OF_MONSTER             0
	char monster[PARAMS_MONSTERS][QUERY_SIZE];
	for ( int i = 0; i < PARAMS_MONSTERS; i++ ) {
		if ( read_param ( monster[i], &s ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;
		if ( *s++ != ';' ) return EVENT_EVENT_FALSE;
	}
	if ( strlen ( monster[TYPE_OF_MONSTER] ) > 1 ) return EVENT_EVENT_FALSE;

	if ( monster[TYPE_OF_MONSTER][0] == '0' ) return EVENT_EVENT_FALSE; // если персонаж общий, то ошибка

	/* узнать есть ли такой монстр */
	snprintf ( query, QUERY_SIZE, "select id from personas where persona = '%c';", monster[TYPE_OF_MONSTER][0] );
	mysql_query ( &db, query );
	store = mysql_store_result ( &db );
	length = mysql_num_rows ( store );
	mysql_free_result ( store );
	if ( length == 0 ) return EVENT_EVENT_FALSE;

	/* получить характеристики персонажа */
	snprintf ( query, QUERY_SIZE, "select health, attack, accuracy, agility from initial_characteristics where monster = '%c';",
			monster[TYPE_OF_MONSTER][0]
		 );
	mysql_query ( &db, query );
	store = mysql_store_result ( &db );
	length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }
	row = mysql_fetch_row ( store );
	int health = 0;
	int attack = 0;
	int accuracy = 0;
	int agility = 0;
#define PARAM_HEALTH         0
#define PARAM_ATTACK         1
#define PARAM_ACCURACY       2
#define PARAM_AGILITY        3
	if ( row[PARAM_HEALTH] && row[PARAM_ATTACK] && row[PARAM_ACCURACY] && row[PARAM_AGILITY] ) {
		health = atoi ( row[PARAM_HEALTH] );
		attack = atoi ( row[PARAM_ATTACK] );
		accuracy = atoi ( row[PARAM_ACCURACY] );
		agility = atoi ( row[PARAM_AGILITY] );

		/* если уже записан игрок в таблицу, то больше не записывать */
		snprintf ( query, QUERY_SIZE, "select id from gamers_personas where id_user = '%d';", id_user );
		mysql_query ( &db, query );
		store = mysql_store_result ( &db );
		length = mysql_num_rows ( store );
		mysql_free_result ( store );
		if ( length > 0 ) return EVENT_EVENT_TRUE;

		/* итак характеристики получены, теперь надо записать нового персонажа в таблицу */
		snprintf ( query, QUERY_SIZE, "insert into gamers_personas ( id_user, itype, ilevel, gold, rubies, health, attack, accuracy, agility, spheres, energy, dribs, name ) values "
				"( '%d', '%c', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%d', '%s' );",
				id_user,
				monster[TYPE_OF_MONSTER][0],
				1,
				conf.gamer_gold,
				0,
				health,
				attack,
				accuracy,
				agility,
				0,  // magic spheres
				100, // energy
				0, // dribs
				name
			 );
		mysql_query ( &db, query );
		return EVENT_EVENT_TRUE;
	}

	return EVENT_EVENT_FALSE;
}

int db_register_with_login ( const char *dt, const int fd ) {
	const char *s = dt;
#define PARAMS_REGISTRATION         3
#define REGISTER_LOGIN              0
#define REGISTER_PASSWORD           1
#define REGISTER_NAME               2
#define QUERY_SIZE           4096
	char reg_data[PARAMS_REGISTRATION][QUERY_SIZE];
	for ( int i = 0; i < PARAMS_REGISTRATION; i++ ) {
		if ( read_param ( reg_data[i], &s ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;
		if ( *s++ != ';' ) return EVENT_EVENT_FALSE;
	}
	/* проверить длину пароля, пароль должен быть не менее 6 символов. */
	if ( strlen ( reg_data[REGISTER_PASSWORD] ) < 6 ) return EVENT_EVENT_FALSE;
	/* проверить, есть ли такой логин */
	char query[QUERY_SIZE];
	snprintf ( query, QUERY_SIZE, "select nick from users where nick = '%s';", reg_data[REGISTER_LOGIN] );
	mysql_query ( &db, query );
	MYSQL_RES *store = mysql_store_result ( &db );

	long unsigned int length = mysql_num_rows ( store );
	if ( length > 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }
	mysql_free_result ( store );

	/* зарегестрировать нового пользователя */
	char hash[65];
	get_sha256 ( reg_data[REGISTER_PASSWORD], hash );
	snprintf ( query, QUERY_SIZE, "insert into users ( nick, password, name, with_login, online ) values ( '%s', '%s', '%s', 1, 0 );",
			reg_data[REGISTER_LOGIN],
			hash,
			reg_data[REGISTER_NAME]
		 );
	mysql_query ( &db, query );

	/* получить id_user игрока и user_id */
	snprintf ( query, QUERY_SIZE, "select id, user_id, with_login from users where nick = '%s';", reg_data[REGISTER_LOGIN] );
	mysql_query ( &db, query );
	store = mysql_store_result ( &db );
	length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }
	printf ( "!!\n" );
	MYSQL_ROW row = mysql_fetch_row ( store );
#define PARAM_SIZE_STRING        512
#define REGISTRATION_WITH_LOGIN       1
#define REGISTRATION_WITH_OAUTH       0
	int id_user = atoi ( row[0] );
	int wlogin = atoi ( row[2] );
	char user_id[PARAM_SIZE_STRING];
	switch ( wlogin ) {
		case REGISTRATION_WITH_LOGIN:
			{
				write_to_online_fd ( id_user, fd );
			}
			break;
		case REGISTRATION_WITH_OAUTH:
			{
				strncpy ( user_id, row[1], strlen ( row[1] ) );
				write_to_online_fd_oauth ( id_user, fd, user_id );
			}
			break;
		default:
			mysql_free_result ( store );
			return EVENT_EVENT_FALSE;
			break;
	}

	mysql_free_result ( store );
	return EVENT_EVENT_TRUE;

}

static int buy_check_levels ( const int level, const char *lvl ) {
	int length = strlen ( lvl );
	int sp = 0;
	for ( int i = 0; i < length; i++ ) {
		if ( lvl[i] == '-' ) { sp = 1; break; }
	}
	switch ( sp ) {
		case 0:
			{
				int num = atoi ( lvl );
				if ( level != num ) return EVENT_EVENT_FALSE;
				return EVENT_EVENT_TRUE;
			}
			break;
		case 1:
			{
				int lvl1;
				int lvl2;
				sscanf ( lvl, "%d-%d", &lvl1, &lvl2 );
				if ( level >= lvl1 && level <= lvl2 ) return EVENT_EVENT_TRUE;
				return EVENT_EVENT_FALSE;
			}
			break;
	}
}

int db_get_main_menu ( const int fd ) {
	switch ( check_persona ( fd ) ) {
		case EVENT_EVENT_FALSE:
			return EVENT_EVENT_FALSE;
			break;
		case EVENT_USER_NOT_CREATED:
			write ( fd, "1", 2 );
			return EVENT_EVENT_TRUE;
			break;
		case EVENT_USER_CREATED:
			break;
	}

	/* получить сначала id_user */
	int id_user = check_online_and_get_id_user ( fd );
	if ( id_user == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;

#define RESULT          13
#define TYPE             0
#define LEVEL            1
#define GOLD             2
#define RUBIES           3
#define HEALTH           4
#define ATTACK           5
#define ACCURACY         6
#define AGILITY          7
#define SPHERES          8
#define ENERGY           9
#define DRIBS           10
#define AVATAR          11
#define NAME            12
#define ROW_AVATAR       0
#define ROW_NAME         1
#define ROW_DESCRIBE     0
#define QUERY_SIZE    4096
	char res[RESULT][QUERY_SIZE];
	char query[QUERY_SIZE];

	snprintf ( query, QUERY_SIZE, "select itype, ilevel, gold, rubies, health, attack, accuracy, agility, spheres, energy, dribs from gamers_personas where id_user = '%d';",
			id_user
		 );
	mysql_query ( &db, query );

	MYSQL_RES *store = mysql_store_result ( &db );
	long unsigned int length = mysql_num_rows ( store );
	if ( length == 0 ) {
		mysql_free_result ( store );
		return EVENT_EVENT_FALSE;
	}
	MYSQL_ROW row = mysql_fetch_row ( store );
	strncpy ( res[TYPE], row[TYPE], strlen ( row[TYPE] ) + 1 );
	strncpy ( res[LEVEL], row[LEVEL], strlen ( row[LEVEL] ) + 1 );
	strncpy ( res[GOLD], row[GOLD], strlen ( row[GOLD] ) + 1 );
	strncpy ( res[RUBIES], row[RUBIES], strlen ( row[RUBIES] ) + 1 );
	strncpy ( res[HEALTH], row[HEALTH], strlen ( row[HEALTH] ) + 1 );
	strncpy ( res[ATTACK], row[ATTACK], strlen ( row[ATTACK] ) + 1 );
	strncpy ( res[ACCURACY], row[ACCURACY], strlen ( row[ACCURACY] ) + 1 );
	strncpy ( res[AGILITY], row[AGILITY], strlen ( row[AGILITY] ) + 1 );
	strncpy ( res[SPHERES], row[SPHERES], strlen ( row[SPHERES] ) + 1 );
	strncpy ( res[ENERGY], row[ENERGY], strlen ( row[ENERGY] ) + 1 );
	strncpy ( res[DRIBS], row[DRIBS], strlen ( row[DRIBS] ) + 1 );
	mysql_free_result ( store );

	/* получить описание типа персонажа */
	snprintf ( query, QUERY_SIZE, "select descr from personas where persona = '%c';", res[TYPE][0] );
	mysql_query ( &db, query );
	store = mysql_store_result ( &db );
	length = mysql_num_rows ( store );
	if ( length == 0 ) {
		mysql_free_result ( store );
		return EVENT_EVENT_FALSE;
	}
	row = mysql_fetch_row ( store );
	strncpy ( res[TYPE], row[ROW_DESCRIBE], strlen ( row[ROW_DESCRIBE] ) + 1 );
	mysql_free_result ( store );

	/* ещё надо получить ссылку на аватарку, если есть */
	snprintf ( query, QUERY_SIZE, "select avatar, name from users where id = '%d';", id_user );
	mysql_query ( &db, query );

	store = mysql_store_result ( &db );
	length = mysql_num_rows ( store );
	if ( length == 0 ) {
		mysql_free_result ( store );
		return EVENT_EVENT_FALSE;
	}
	row = mysql_fetch_row ( store );
	if ( row[ROW_AVATAR] ) {
		strncpy ( res[AVATAR], row[ROW_AVATAR], strlen ( row[ROW_AVATAR] ) + 1 );
	} else {
		res[AVATAR][0] = 0;
	}

	strncpy ( res[NAME], row[ROW_NAME], strlen ( row[ROW_NAME] ) + 1 );

	/* теперь надо отправить данные игроку */
	snprintf ( query, QUERY_SIZE, "2;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;",
			res[TYPE],      // 1
			res[LEVEL],     // 2
			res[GOLD],      // 3
			res[RUBIES],    // 4
			res[HEALTH],    // 5
			res[ATTACK],    // 6
			res[ACCURACY],  // 7
			res[AGILITY],   // 8
			res[SPHERES],   // 9
			res[ENERGY],    // 10
			res[DRIBS],     // 11
			res[NAME],      // 12
			res[AVATAR]     // 13
		 );
	write ( fd, query, strlen ( query ) + 1 );
	return EVENT_EVENT_TRUE;

}

int select_persona ( const char *dt, const int fd ) {
	const char *s = dt;
	char persona[DB_PARAM_SIZE];
	if ( read_param ( &persona[0], &s ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;
	if ( *s++ != ';' ) return EVENT_EVENT_FALSE;
	if ( check_num ( persona ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;
	int num_persona = atoi ( persona );
	printf ( "persona: %d\n", num_persona );

#define QUERY_SIZE           4096
	/* получить id_user игрока */
	char query[QUERY_SIZE];
	snprintf ( query, QUERY_SIZE, "select id_user from online where fd = %d;", fd );
	mysql_query ( &db, query );
	MYSQL_RES *store = mysql_store_result ( &db );

	long unsigned int length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }

	MYSQL_ROW row = mysql_fetch_row ( store );
	if ( row == NULL ) {
		mysql_free_result ( store );
		return EVENT_EVENT_FALSE;
	}
#define ONLINE_ID_USER            0

	int id_user = atoi ( row[ONLINE_ID_USER] );
	mysql_free_result ( store );

	/* был ли выбран уже персонаж */
	snprintf ( query, QUERY_SIZE, "select itype from gamers_personas where id_user = %d;", id_user );
	mysql_query ( &db, query );
	store = mysql_store_result ( &db );
	length = mysql_num_rows ( store );
	mysql_free_result ( store );
	/* если запись есть, то не выбирать персонажа заного */
	if ( length > 0 ) return EVENT_EVENT_FALSE;

	/* создать персонажа */
	snprintf ( query, QUERY_SIZE, "insert into gamers_personas ( id_user, itype, ilevel, gold, rubies ) values "
			"( '%d', '%d', '1', '%d', '0' );",
			id_user,
			num_persona,
			conf.gamer_gold
		 );
	mysql_query ( &db, query );

	return EVENT_EVENT_TRUE;
}

#define PERSONAS_PERSONA                 0
#define PERSONAS_LEVEL                   1
#define PERSONAS_GOLD                    2
#define PERSONAS_RUBIES                  3
#define PERSONAS_COUNT                   4
#define PERSONAS_ITEMS                   4

/* покупка предметов */
int buy_items ( const char *dt, const int fd ) {
	const char *s = dt;
	char type_of_money[DB_PARAM_SIZE];
	if ( read_param ( &type_of_money[0], &s ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;
	if ( *s++ != ';' ) return EVENT_EVENT_FALSE;

	char count_of_params[DB_PARAM_SIZE];
	if ( read_param ( &count_of_params[0], &s ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;
	if ( *s++ != ';' ) return EVENT_EVENT_FALSE;
	if ( check_num ( &count_of_params[0] ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;
	int count = atoi ( count_of_params );

	char params[count][DB_PARAM_SIZE];
	int i = 0;

	for ( int i = 0; i < count; i++ ) {
		if ( read_param ( &params[i][0], &s ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;
		if ( *s++ != ';' ) return EVENT_EVENT_FALSE;
		check_param ( &params[i][0] );
	}

	/* получить стоимость в золоте или в рубинах */
#define MONEY_GOLD          '0'
#define MONEY_RUBIES        '1'
	int money;
	switch ( type_of_money[0] ) {
		case MONEY_GOLD: money = MONEY_GOLD; break;
		case MONEY_RUBIES: money = MONEY_RUBIES; break;
		default:
		   return EVENT_EVENT_FALSE;
	}

	/* получить id игрока */
	char query[DB_SIZE_QUERY];
	snprintf ( query, DB_SIZE_QUERY, "select id_user from online where fd = '%d';", fd );
	mysql_query ( &db, query );

	MYSQL_RES *store = mysql_store_result ( &db );

	long unsigned int length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }

	MYSQL_ROW row = mysql_fetch_row ( store );
	if ( row == NULL ) {
		mysql_free_result ( store );
		return EVENT_EVENT_FALSE;
	}

#define RES_ID_USER               0
	int id_user = atoi ( row[RES_ID_USER] );
	mysql_free_result ( store );

	int personas[PERSONAS_COUNT];
	/* узнать сколько денег у игрока, какой уровень, тип персонажа */
	snprintf ( query, DB_SIZE_QUERY, "select itype, ilevel, gold, rubies, items from gamers_personas where id_user = %d;", id_user );
	mysql_query ( &db, query );
	store = mysql_store_result ( &db );

	length = mysql_num_rows ( store );
	if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }

	row = mysql_fetch_row ( store );
	if ( row == NULL ) {
		mysql_free_result ( store );
		return EVENT_EVENT_FALSE;
	}
	personas[PERSONAS_PERSONA] = atoi ( row[PERSONAS_PERSONA] );
	personas[PERSONAS_LEVEL]   = atoi ( row[PERSONAS_LEVEL] );
	personas[PERSONAS_GOLD]    = atoi ( row[PERSONAS_GOLD] );
	personas[PERSONAS_RUBIES]  = atoi ( row[PERSONAS_RUBIES] );
#define ITEMS_SIZE            1024
	char items[ITEMS_SIZE];
	items[0] = 0;
	if ( row[PERSONAS_ITEMS] != NULL ) {
		strncpy ( &items[0], row[PERSONAS_ITEMS], strlen ( row[PERSONAS_ITEMS] ) );
	}
	mysql_free_result ( store );

	struct items {
		int count;
		int i[512];
	} it;

	void parse_items ( ) {
		it.count = 0;
		int length = strlen ( items );
		if ( length == 0 ) return;
		char itm[5];
		char *s = &items[0];
		char *st = s;
		int sp = 0;
		for ( int i = 0; i < length; i++, s++ ) {
			if ( *s == ',' ) {
				sp = 1;
				int len = s - st;
				strncpy ( &itm[0], st, len );
				itm[len] = 0;
				it.i[it.count++] = atoi ( itm );
				st = s + 1;
			}
		}
		if ( sp == 1 ) {
			int len = s - st;
			strncpy ( &itm[0], st, len );
			itm[len] = 0;
			it.i[it.count++] = atoi ( itm );
		}
		if ( sp == 0 && length > 0 ) {
			it.count = 1;
			it.i[0] = atoi ( items );
		}
	}

	parse_items ( );

#define RESULT_PERSONA           0
#define RESULT_LEVEL             1
#define RESULT_GOLD              2
#define RESULT_RUBIES            3
#define RESULT_ID                4

#define TYPE_PERSONA_SHARED      0
#define TYPE_PERSONA_WARLOCK     1
#define TYPE_PERSONA_BANDIT      2
#define TYPE_PERSONA_DEAD_MAN    3
	/* проверить возможность купить предметы */
	for ( int i = 0; i < count; i++ ) {
		printf ( "params: %s\n", params[i] );
		snprintf ( query, DB_SIZE_QUERY, "select persona, ilevel, gold, rubies, id from items where icon = '%s';", params[i] );
		mysql_query ( &db, query );
		MYSQL_RES *store = mysql_store_result ( &db );

		long unsigned int length = mysql_num_rows ( store );
		if ( length == 0 ) { mysql_free_result ( store ); return EVENT_EVENT_FALSE; }

		MYSQL_ROW row = mysql_fetch_row ( store );
		if ( row == NULL ) {
			mysql_free_result ( store );
			return EVENT_EVENT_FALSE;
		}

		int res_persona = atoi ( row[RESULT_PERSONA] );
		int id_item = atoi ( row[RESULT_ID] );

		char res_level[33];
		strncpy ( &res_level[0], row[RESULT_LEVEL], strlen ( row[RESULT_LEVEL] ) );
		int res_gold = atoi ( row[RESULT_GOLD] );
		int res_rubies = 0;
		if ( row[RESULT_RUBIES] == NULL ) {
			res_rubies = 0;
		} else res_rubies = atoi ( row[RESULT_RUBIES] );
		mysql_free_result ( store );
		/* проверить, подходит ли к типу игрока */
		switch ( res_persona ) {
			case TYPE_PERSONA_SHARED:
				break;
			case TYPE_PERSONA_WARLOCK:
			case TYPE_PERSONA_BANDIT:
			case TYPE_PERSONA_DEAD_MAN:
				if ( personas[PERSONAS_PERSONA] != res_persona ) return EVENT_EVENT_FALSE;
				break;
			default:
				return EVENT_EVENT_FALSE;
		}
		/* проверить уровень игрока */
		if ( buy_check_levels ( personas[PERSONAS_LEVEL], res_level ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;
		/* проверить доступность денег */
		if ( res_gold > 0 ) money = MONEY_GOLD;
		if ( res_rubies > 0 ) money = MONEY_RUBIES;
		switch ( money ) {
			case MONEY_GOLD:
				{
					if ( personas[PERSONAS_GOLD] < res_gold ) return EVENT_EVENT_FALSE;
					personas[PERSONAS_GOLD] -= res_gold;
				}
				break;
			case MONEY_RUBIES:
				{
					if ( personas[PERSONAS_RUBIES] < res_rubies ) return EVENT_EVENT_FALSE;
					personas[PERSONAS_RUBIES] -= res_rubies;
				}
				break;
		}
		/* проверить есть ли такой предмет уже в списке */
		for ( int i = 0; i < it.count; i++ ) {
			if ( it.i[i] == id_item ) return EVENT_EVENT_FALSE;
		}
		it.i[it.count++] = id_item;
	}

	/* записать новое значение о текущем состоянии золота и рубинов 
	 * сначала составить строку перечисление купленных предметов */
	memset ( &items[0], 0, ITEMS_SIZE );

	{
		char *d = &items[0];
		int total = 0;
		int n;
		for ( int i = 0; i < it.count; i++ ) {
			sprintf ( d, "%d%s%n", it.i[i], i == it.count - 1 ? "" : ",", &n );
			d += n;
			total += n;
			if ( total > ITEMS_SIZE ) return EVENT_EVENT_FALSE;
		}
	}
#define QUERY_BUFFER_SIZE              4096
	{
		snprintf ( query, QUERY_BUFFER_SIZE, "update gamers_personas set items = '%s' where id_user = %d;", items, id_user );
		mysql_query ( &db, query );
		char query[QUERY_BUFFER_SIZE];
		snprintf ( query, QUERY_BUFFER_SIZE, "update gamers_personas set gold = %d where id_user = %d;", personas[PERSONAS_GOLD], id_user );
		mysql_query ( &db, query );
		snprintf ( query, QUERY_BUFFER_SIZE, "update gamers_personas set rubies = %d where id_user = %d;", personas[PERSONAS_RUBIES], id_user );
		mysql_query ( &db, query );
	}

	return EVENT_EVENT_TRUE;
}

#define REG_USER_ID                 0
#define REG_NAME                    1
#define REG_NICK                    2
#define REG_COUNTRY                 3
#define REG_CITY                    4
#define REG_SEX                     5
#define REG_BIRTHDAY                6
#define REG_AVATAR                  7
#define REG_DATE_REG                8
#define REG_LENGTH                  9

int db_registration ( const char *dt, const int fd ) {
	char params[REG_LENGTH][DB_PARAM_SIZE];
	int i = 0;
	const char *s = dt;

	for ( int i = 0; i < REG_LENGTH; i++ ) {
		if ( read_param ( &params[i][0], &s ) == EVENT_EVENT_FALSE ) return EVENT_EVENT_FALSE;
		if ( *s++ != ';' ) return EVENT_EVENT_FALSE;
		check_param ( &params[i][0] );
	}

	/* проверить на числа */
	int numbers[] = { REG_USER_ID, REG_COUNTRY, REG_CITY, REG_SEX };
	int size_of_number = sizeof ( numbers ) / 4;
	for ( int i = 0; i < size_of_number; i++ ) {
		switch ( check_num ( &params[numbers[i]][0] ) ) {
			case EVENT_EVENT_FALSE: return EVENT_EVENT_FALSE;
			default: break;
		}
	}

	/* узнать, онлайн ли пользователь */
	char query[DB_SIZE_QUERY];
	snprintf ( query, DB_SIZE_QUERY, "select fd from online where user_id = '%s';", params[REG_USER_ID] );
	mysql_query ( &db, query );

	MYSQL_RES *store = mysql_store_result ( &db );

	long unsigned int length = mysql_num_rows ( store );
	if ( length > 0 ) {
		mysql_free_result ( store );
		return EVENT_USER_IS_ONLINE;
	}
	mysql_free_result ( store );

	snprintf ( query, DB_SIZE_QUERY,
			"select id from users where user_id = '%s';",
			params[REG_USER_ID]
		 );
	mysql_query ( &db, query );
	store = mysql_store_result ( &db );
	length = mysql_num_rows ( store );
	mysql_free_result ( store );
	if ( length > 0 ) goto reg_end;

	/* сделать запись в таблицу */
	snprintf ( query, DB_SIZE_QUERY,
			"insert into users ( user_id, name, nick, country, city, sex, birthday, avatar, date_reg, with_login, online ) "
			"values ( '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', 0, 0 );",
			params[REG_USER_ID],
			params[REG_NAME],
			params[REG_NICK],
			params[REG_COUNTRY],
			params[REG_CITY],
			params[REG_SEX],
			params[REG_BIRTHDAY],
			params[REG_AVATAR],
			params[REG_DATE_REG]
		 );
	mysql_query ( &db, query );

reg_end:
	store = NULL;

	snprintf ( query, DB_SIZE_QUERY, "select id from users where user_id = '%s';", params[REG_USER_ID] );
	mysql_query ( &db, query );

	store = mysql_store_result ( &db );

	length = mysql_num_rows ( store );
	if ( length == 0 ) {
		mysql_free_result ( store );
		return EVENT_EVENT_FALSE;
	}

	MYSQL_ROW row = mysql_fetch_row ( store );
	if ( row == NULL ) {
		mysql_free_result ( store );
		return EVENT_EVENT_FALSE;
	}

	unsigned long *lengths;
	lengths = mysql_fetch_lengths ( store );
	char id[DB_PARAM_SIZE];
	length = (long unsigned int) lengths[0];
	strncpy ( &id[0], row[0], length + 1 );
	mysql_free_result ( store );

	/* записать user_id игрока и его файловый дескриптор для связи */
	int id_user = atoi ( params[REG_USER_ID] );
	write_to_online_fd_oauth ( id_user, fd, params[REG_USER_ID] );

	return EVENT_EVENT_TRUE;
 }
