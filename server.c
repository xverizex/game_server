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
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "db.h"
#include "config.h"
#include "events.h"

/* сокет сервера */
static int sockfd;
extern struct conf conf;
struct sockaddr_in in_server;
int epollfd, nfds;
#define AUTH_ADMIN            0
#define AUTH_GAMER            1
char sym[2] = { '!', '1' };

#define DEFAULT_SIZE_POLL        65535
#define DEFAULT_SIZE_DATA         1024

struct epoll_event ev, events[DEFAULT_SIZE_POLL];
char gamers[DEFAULT_SIZE_POLL];

static void *thread_control_clients ( void *data ) {

	while ( 1 ) {
		nfds = epoll_wait ( epollfd, events, DEFAULT_SIZE_POLL, -1 );
		if ( nfds == 0 ) continue;
		if ( nfds == -1 ) {
			perror ( "poll" );
			exit ( EXIT_FAILURE );
		}
		for ( int i = 0; i < nfds; i++ ) {
			char data[DEFAULT_SIZE_DATA + 1];
			memset ( &data[0], 0, DEFAULT_SIZE_DATA + 1 );
			int ret = read ( events[i].data.fd, data, DEFAULT_SIZE_DATA );
			if ( ret <= 0 ) {
				int fd = events[i].data.fd;
				if ( gamers[fd] == sym[AUTH_GAMER] ) db_offline_user ( events[i].data.fd );
				epoll_ctl ( epollfd, EPOLL_CTL_DEL, events[i].data.fd, &events[i] );
				gamers[fd] = 0;
				close ( fd );
				printf ( "игрок %d отключился.\n", fd );
				/* здесь отправить другому игроку о том что игрок вышел из боя, если проводится бой. */
				continue;
			}
			switch ( data[0] ) {
				case '!':
					{
						int ret = db_register_admin ( &data[1] );
						switch ( ret ) {
							case EVENT_EVENT_FALSE:
								/* можно вывести сообщение об ошибке */
								write ( events[i].data.fd, "0", 2 );
								epoll_ctl ( epollfd, EPOLL_CTL_DEL, events[i].data.fd, &events[i] );
								close ( events[i].data.fd );
								break;
							default:
								{
									int fd = events[i].data.fd;
									gamers[fd] = sym[AUTH_ADMIN];
									write ( events[i].data.fd, "!", 2 );
								}
								break;
						}
					}
					break;
				case '0': // регистрация персонажа с помощью vk
					{
						int ret = db_registration ( &data[1], events[i].data.fd );
						switch ( ret ) {
							case EVENT_USER_IS_ONLINE:
								/* отправить сообщение что пользователь онлайн
								 * оборвать соединение */
								epoll_ctl ( epollfd, EPOLL_CTL_DEL, events[i].data.fd, &events[i] );
								write ( events[i].data.fd, "0", 2 );
								close ( events[i].data.fd );
								printf ( "пользователь уже есть в игре %d\n", events[i].data.fd );
								continue;
								break;
							case EVENT_EVENT_FALSE:
								/* неправильные данные */
								printf ( "неправильные данные.\n" );
								epoll_ctl ( epollfd, EPOLL_CTL_DEL, events[i].data.fd, &events[i] );
								write ( events[i].data.fd, "0", 2 );
								close ( events[i].data.fd );
								break;
							default:
								{
									int fd = events[i].data.fd;
									gamers[fd] = sym[AUTH_GAMER];
									write ( events[i].data.fd, "1", 2 );
								}
								break;
						}
					}
					break;
				case '1': // создание персонажа
					{
						int fd = events[i].data.fd;
						if ( gamers[fd] != sym[AUTH_GAMER] ) break;
						int ret = select_persona ( &data[1], fd );
						switch ( ret ) {
							case EVENT_EVENT_FALSE:
								/* неправильные данные */
								printf ( "неправильные данные.\n" );
								write ( fd, "0", 2 );
								break;
							default:
								{
									write ( fd, "1", 2 );
								}
								break;
						}
					}
					break;
				case '2': // покупка предметов
					{
						int fd = events[i].data.fd;
						if ( gamers[fd] != sym[AUTH_GAMER] ) break;
						int ret = buy_items ( &data[1], fd );
						switch ( ret ) {
							case EVENT_EVENT_FALSE:
								/* неправильные данные */
								printf ( "неправильные данные.\n" );
								write ( fd, "0", 2 );
								break;
							default:
								{
									write ( fd, "a", 2 );
								}
								break;
						}
					}
					break;
				case '3': // авторизация игрока с помощью логина и пароля
					{
						int fd = events[i].data.fd;
						int ret = db_auth_with_login ( &data[1], fd );
						switch ( ret ) {
							case EVENT_EVENT_TRUE:
								gamers[fd] = sym[AUTH_GAMER];
								printf ( "успешная авторизация %d\n", fd );
								break;
							case EVENT_EVENT_DISCONNECT:
								epoll_ctl ( epollfd, EPOLL_CTL_DEL, fd, &events[i] );
								write ( events[i].data.fd, "0", 2 );
								close ( events[i].data.fd );
								printf ( "пользователь уже есть в игре %d\n", fd );
								break;
							case EVENT_EVENT_FALSE:
								printf ( "ошибка в авторизации %d\n", fd );
								write ( fd, "0", 2 );
								break;
						}
					}
					break;
				case '4': // регистрация нового игрока с помощью логина и пароля
					{
						int fd = events[i].data.fd;
						int ret = db_register_with_login ( &data[1], fd );
						int result;
						switch ( ret ) {
							case EVENT_EVENT_TRUE: // успешная регистрация
								printf ( "успешная регистрация %d\n", fd );
								gamers[fd] = sym[AUTH_GAMER];
								result = write ( fd, "1", 2 );
								break;
							case EVENT_EVENT_FALSE:
								printf ( "ошибка в регистрации %d\n", fd );
								write ( fd, "3", 2 ); // отправить ошибку в регистрации.
								break;
						}
					}
					break;
				case '5': // получить информацию о начальных характеристиках персонажа
					{
						int fd = events[i].data.fd;
						if ( gamers[fd] != sym[AUTH_GAMER] ) { write ( fd, "0", 2 ); break; }
						int ret = db_get_characteristics ( &data[1], fd );
						switch ( ret ) {
							case EVENT_EVENT_TRUE:
								printf ( "успешное получение начальных характеристик\n" );
								break;
							case EVENT_EVENT_FALSE:
								printf ( "ошибка при получении начальных характеристик\n" );
								write ( fd, "0", 2 );
								break;
						}
					}
					break;
				case '6': // выбор этого персонажа и запись в базу данных
					{
						int fd = events[i].data.fd;
						if ( gamers[fd] != sym[AUTH_GAMER] ) { write ( fd, "0", 2 ); break; }
						int ret = db_write_persona_new ( &data[1], fd );
						switch ( ret ) {
							case EVENT_EVENT_TRUE:
								printf ( "успешная запись нового персонажа\n" );
								write ( fd, "1", 2 );
								break;
							case EVENT_EVENT_FALSE:
								printf ( "ошибка при записи нового персонажа\n" );
								write ( fd, "0", 2 );
								break;
						}
					}
					break;
				case '7': // запрос на данные в главном меню
					{
						int fd = events[i].data.fd;
						if ( gamers[fd] != sym[AUTH_GAMER] ) { write ( fd, "0", 2 ); break; }
						int ret = db_get_main_menu ( fd );
						switch ( ret ) {
							case EVENT_EVENT_TRUE:
								printf ( "успешное получение инфы в главном меню\n" );
								break;
							case EVENT_EVENT_FALSE:
								printf ( "ошибка при получении инфы\n" );
								write ( fd, "0", 2 );
								break;
						}
					}
					break;
				case '8': // получить информацию о сумме денег и доступных оружиях
					{
						int fd = events[i].data.fd;
						if ( gamers[fd] != sym[AUTH_GAMER] ) { write ( fd, "0", 2 ); break; }
						int ret = db_get_money_and_shop ( fd );
						switch ( ret ) {
							case EVENT_EVENT_TRUE:
								printf ( "успешное получение денег\n" );
								break;
							case EVENT_EVENT_FALSE:
								printf ( "ошибка при получении денег\n" );
								write ( fd, "0", 2 );
								break;
						}

					}
					break;
				case '9': // меню поиска противников. получить информацию о характеристиках своего персонажа и персонажей противников.
					{
						int fd = events[i].data.fd;
						if ( gamers[fd] != sym[AUTH_GAMER] ) { write ( fd, "0", 2 ); break; }
						int ret = db_get_characteristics_menu_fight ( fd );
						switch ( ret ) {
							case EVENT_EVENT_TRUE:
								printf ( "успешное получение информации в меню поиска противников\n" );
								break;
							case EVENT_EVENT_FALSE:
								printf ( "ошибка при получении информации в меню поиска противников\n" );
								write ( fd, "0", 2 );
								break;
						}
					}
					break;
			}
		}
	}
}

/* создание потока */
void create_thread ( ) {
	epollfd = epoll_create1 ( 0 );
	if ( epollfd == -1 ) {
		perror ( "epoll_create" );
		exit ( EXIT_FAILURE );
	}

	pthread_t t;
	pthread_create ( &t, NULL, thread_control_clients, NULL );
}

void wait_client ( ) {
	socklen_t soss = sizeof ( in_server );
	int client = accept ( sockfd, ( struct sockaddr * ) &in_server, &soss );
	if ( client == -1 ) return;
	printf ( "%d подключен.\n", client );
	ev.events = EPOLLIN;
	ev.data.fd = client;
	if ( epoll_ctl ( epollfd, EPOLL_CTL_ADD, client, &ev ) == -1 ) {
		perror ( "epoll_ctl" );
		exit ( EXIT_FAILURE );
	}
}

/* конфигурация сокета */
void configure_socket ( ) {
	 sockfd = socket ( AF_INET, SOCK_STREAM, 0 );
	 if ( sockfd == -1 ) {
		 perror ( "socket" );
		 exit ( EXIT_FAILURE );
	 } 

	 int ret;
	 {
		int opt = 1;
	 	ret = setsockopt ( sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof ( opt ) );
		if ( ret == -1 ) {
			perror ( "sock opt SO_REUSEPORT" );
			exit ( EXIT_FAILURE );
		}
	 }

	 in_server.sin_family = AF_INET;
	 in_server.sin_port = htons ( conf.port );
	 inet_aton ( "0.0.0.0", &in_server.sin_addr );
	// in_server.sin_addr.s_addr = 0;
	 ret = bind ( sockfd, ( struct sockaddr *) &in_server, sizeof ( in_server ) );
	 if ( ret == -1 ) {
		 perror ( "bind" );
		 exit ( EXIT_FAILURE );
	 }
	 ret = listen ( sockfd, conf.listen );
	 if ( ret == -1 ) {
		 perror ( "listen" );
		 exit ( EXIT_FAILURE );
	 }

}
