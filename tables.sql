drop table if exists users;
drop table if exists online;
drop table if exists admin;
drop table if exists items;
drop table if exists personas;
drop table if exists gamers_personas;
drop table if exists initial_characteristics;

create table if not exists users (
	id        int not null auto_increment primary key,
	user_id   varchar(255) null,
	name      varchar(255) not null,
	nick      varchar(255) not null,
	password  varchar(255) null,
	country   int null,
	city      int null,
	sex       int null,
	birthday  varchar(255) null,
	avatar    varchar(255) null,
	date_reg  varchar(255) null,
	with_login int not null, # если 1, то с помощью логина и пароля, если 0, то с помощью oauth2
	online    int not null,
	index ( id, user_id, nick )
	) engine=InnoDB default charset=utf8;

create table if not exists online (
	id       int not null auto_increment primary key,
	id_user  int not null,
	user_id  int null,
	fd       int not null,
	index ( fd )
	) engine=InnoDB default charset=utf8;

create table if not exists gamers_personas (
	id       int not null auto_increment primary key,
	id_user  int not null,
	name     varchar(255) not null,
	itype    int not null,
	ilevel    int not null,
	gold     int not null,
	rubies   int not null,
	health   int not null,
	attack   int not null,
	accuracy int not null,
	agility  int not null,
	spheres  int not null,
	energy   int not null,
	dribs    int not null,
	items    text(1024) null
	) engine=InnoDB default charset=utf8;

create table if not exists admin (
	id       int not null auto_increment primary key,
	login    varchar(255) not null,
	passwd   varchar(255) not null
	) engine=InnoDB default charset=utf8;

create table if not exists personas (
	id       int not null auto_increment primary key,
	persona  int not null,
	descr    varchar(255) not null,
	index ( descr )
	) engine=InnoDB default charset=utf8;

insert into personas ( persona, descr ) values
( 0, 'Общее' ),
( 1, 'Разбойник' ),
( 2, 'Колдун' ),
( 3, 'Мертвец' );

create table if not exists initial_characteristics (
	id       int not null auto_increment primary key,
	monster  int not null,
	health   int not null,
	attack   int not null,
	accuracy int not null,
	agility  int not null
	) engine=InnoDB default charset=utf8;

insert into initial_characteristics ( monster, health, attack, accuracy, agility ) values 
( 1, 55, 5, 21, 12 ), # Разбойник
( 2, 50, 6, 11, 11 ), # Колдун
( 3, 60, 7, 10, 10 ); # Мертвец

create table if not exists items (
	id       int not null auto_increment primary key,
	itype     varchar(255) not null,
	name     varchar(255) not null,
	persona  int not null,
	ilevel    varchar(32) not null,
	descr    varchar(255) not null,
	gold     int null,
	rubies   int null,
	attack   varchar(255) null,
	parts    int null,
	persent  int null,
	time     varchar(32) null,
	icon     varchar(255) not null,
	num      int not null
	) engine=InnoDB default charset=utf8;

# добавить оружие
insert into items ( 
  itype,          name,                    persona, ilevel,  descr,       gold,   attack,              num,        icon                  ) values 
( 'Холодное',     'Нож',                   0,       '1',     'arm-0',     100,    '5',                 1,          'Ico_knife'           ),
( 'Стрелковое',   'Пистолет простой',      0,       '1',     'arm-1',     250,    '10(+3)',            2,          'Ico_pistol'          ),
( 'Холодное',     'Меч',                   0,       '1',     'arm-2',     150,    '7',                 3,          'Ico_sword'           ),
( 'Стрелковое',   'Двойной пистолет',      0,       '1',     'arm-3',     340,    '15',                4,          'Ico_pistolDouble'    ),
( 'Холодное',     'Тесак',                 0,       '1',     'arm-4',     130,    '6',                 5,          'Ico_backsword'       );
