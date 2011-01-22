#ifndef _PARSE_H
#define _PARSE_H

#define MAX_TO_ADDRESSES 250
#define MAX_MAIL_PARTS   10

typedef struct {
	char *to[MAX_TO_ADDRESSES];
	char *from;
	char *topic;
	char *date;
	char *replyto;
	char *html[MAX_MAIL_PARTS];
	char *text[MAX_MAIL_PARTS];
	char *attach[MAX_MAIL_PARTS];
	char *raw;
} MailParsed;


char *parse_message(char *raw, unsigned long len, MailParsed *mp);
void parse_free(MailParsed *mp);
int isip(char *buf);
void init_date_time(CDATE *date, CTIME *time);
int count_lines(char *buf);
void make_reply(char *raw, char *newtext);

#endif
