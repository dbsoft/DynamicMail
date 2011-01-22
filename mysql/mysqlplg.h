#ifndef _MYSQLPLG_H
#define _MYSQLPLG_H

typedef struct {
	int         Type;
	char        *name;
} AccountStruct;

typedef struct {
	char         hostname[100];
	char         username[100];
	char         password[100];
} LoginStruct;

#define MYSQL_DATABASE "CREATE DATABASE `DynamicMail`"

#define MYSQL_TABLE1 "CREATE TABLE `Accounts` (\n" \
	"  `entry` int(11) NOT NULL auto_increment,\n" \
	"  `name` text NOT NULL,\n" \
	"  `recvhosttype` int(11) NOT NULL default '0',\n" \
	"  `recvhostname` text NOT NULL,\n" \
	"  `recvhostport` int(11) NOT NULL default '0',\n" \
	"  `recvhostuser` text NOT NULL,\n" \
	"  `recvhostpass` text NOT NULL,\n" \
	"  `sendhosttype` int(11) NOT NULL default '0',\n" \
	"  `sendhostname` text NOT NULL,\n" \
	"  `sendhostport` int(11) NOT NULL default '0',\n" \
	"  `sendhostuser` text NOT NULL,\n" \
	"  `sendhostpass` text NOT NULL,\n" \
	"  `useremail` text NOT NULL,\n" \
	"  `userrealname` text NOT NULL,\n" \
	"  `replyemail` text NOT NULL,\n" \
	"  `replyrealname` text NOT NULL,\n" \
	"  PRIMARY KEY  (`entry`)\n" \
	") TYPE=MyISAM AUTO_INCREMENT=1"

#define MYSQL_TABLE2 "CREATE TABLE `Body` (\n" \
	"  `entry` bigint(20) NOT NULL auto_increment,\n" \
	"  `messid` bigint(20) NOT NULL default '0',\n" \
	"  `body` longtext NOT NULL,\n" \
	"  PRIMARY KEY  (`entry`)\n" \
	") TYPE=MyISAM AUTO_INCREMENT=1"


#define MYSQL_TABLE3 "CREATE TABLE `Folders` (\n" \
	"  `entry` int(11) NOT NULL auto_increment,\n" \
	"  `name` text NOT NULL,\n" \
	"  `folder` text NOT NULL,\n" \
	"  `parent` text NOT NULL,\n" \
	"  `flags` int(11) NOT NULL default '0',\n" \
	"  PRIMARY KEY  (`entry`)\n" \
	") TYPE=MyISAM AUTO_INCREMENT=1"

#define MYSQL_TABLE4 "CREATE TABLE `Mail` (\n" \
	"  `entry` bigint(20) NOT NULL auto_increment,\n" \
	"  `size` int(11) NOT NULL default '0',\n" \
	"  `datetime` timestamp(14) NOT NULL,\n" \
	"  `topic` text NOT NULL,\n" \
	"  `to` text NOT NULL,\n" \
	"  `from` text NOT NULL,\n" \
	"  `folder` int(11) NOT NULL default '0',\n" \
	"  PRIMARY KEY  (`entry`)\n" \
	") TYPE=MyISAM AUTO_INCREMENT=2"
#endif
