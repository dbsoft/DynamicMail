/*
 * This file deals with parsing the message body.
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dwcompat.h>
#include <dw.h>
#include "backend.h"
#include "dmail.h"
#include "parse.h"

int count_lines(char *buf)
{
	int count = 1;

	if(!*buf)
		return 0;

	while(*buf)
	{
		if(*buf == '\n')
			count++;
		buf++;
	}
	return count;
}

/* Makes a reply buffer with quoted text */
void make_reply(char *raw, char *newtext)
{
	int z, len, newpos = 0, totlen = strlen(raw);
	char *start = raw;

	for(z=0;z<totlen;z++)
	{
		/* Found a new line */
		if(raw[z] == '\r')
		{
			int len;

			if(raw[z] == '\r' && raw[z+1] == '\n')
				z++;

			newtext[newpos] = '>';
			newpos++;

			len = &raw[z+1] - start;

			memcpy(&newtext[newpos], start, len);
			newpos += len;

			start = &raw[z+1];
		}
	}
	len = &raw[z+1] - start;

	if(len > 0)
	{
		newtext[newpos] = '>';
		newpos++;

		memcpy(&newtext[newpos], start, len);
	}
}

/* Free memory allocated during parsing */
void parse_free(MailParsed *mp)
{
	int z = 0;

	if(mp->date)
		free(mp->date);
	if(mp->replyto)
		free(mp->replyto);
	if(mp->from)
		free(mp->from);
	if(mp->topic)
		free(mp->topic);
	while(mp->to[z] && z < MAX_TO_ADDRESSES)
	{
		free(mp->to[z]);
		z++;
	}
	z=0;
	while(mp->html[z] && z < MAX_MAIL_PARTS)
	{
		free(mp->html[z]);
		z++;
	}
	z=0;
	while(mp->text[z] && z < MAX_MAIL_PARTS)
	{
		free(mp->text[z]);
		z++;
	}
	z=0;
	while(mp->attach[z] && z < MAX_MAIL_PARTS)
	{
		free(mp->attach[z]);
		z++;
	}
}

/* Get multiple To: addresses from a single line */
void parse_to(MailParsed *mp, int *tocount, char *start, int itemlen)
{
	int z, thisstart = 0, len;

	for(z=0;z<itemlen;z++)
	{
		if(start[z] == ';' || start[z] == ',')
		{
			len = z - thisstart;

			if(len > 0)
			{
				mp->to[*tocount] = calloc(1, len + 1);
				strncpy(mp->to[*tocount], &start[thisstart], len);
				(*tocount)++;
			}
			/* Skip over the trailing space */
			z++;
			thisstart = z + 1;
		}
	}
	len = z - thisstart;

	if(len > 0)
	{
		mp->to[*tocount] = calloc(1, len + 1);
		strncpy(mp->to[*tocount], &start[thisstart], len);
		(*tocount)++;
	}
}

#define TYPE_TEXT 0
#define TYPE_HTML 1
#define TYPE_ATTA 2
#define TYPE_OTHR 3

/* Determine where this part of a multipart message goes, TEXT, HTML or ATTACHMENT */
void check_part(MailParsed *mp, char **start, char *part, char *raw, char **ret, int *rettype, int type, int *textcount, int *htmlcount, int *attachcount)
{
	/* If we aren't in the headers save the
	 * last component whereever necessary.
	 */
	if(part)
	{
		int partlen = *start - part;

		if(type == TYPE_TEXT || type == TYPE_HTML)
		{
			/* If we are generating a MailParsed entry then we
			 * can't touch the actual raw buffer.
			 */
			if(mp && type == TYPE_TEXT)
			{
				if(partlen > 0 && *textcount < MAX_MAIL_PARTS)
				{
					mp->text[*textcount] = calloc(1, partlen+1);
					memcpy(mp->text[*textcount], part, partlen);
					(*textcount)++;
				}
			}
			if(!mp)
			{
				/* If we aren't generating a MailParsed entry then
				 * we can NULL terminate the entry and return it!
				 */
				**start = 0;

				/* If it's the first text entry set the return
				 * buffer to the part.
				 */
				if(*ret == raw || (*rettype != TYPE_TEXT && type == TYPE_TEXT))
				{
					*ret = part;
					*rettype = type;
				}
			}
		}
		/* HTML and Attachment items should only be parsed when
		 * generating a MailParsed struct, unlike the text type.
		 */
		if(mp)
		{
			if(type == TYPE_HTML)
			{
				if(partlen > 0 && *htmlcount < MAX_MAIL_PARTS)
				{
					mp->html[*htmlcount] = calloc(1, partlen+1);
					memcpy(mp->html[*htmlcount], part, partlen);
					(*htmlcount)++;
				}
			}
			if(type == TYPE_ATTA)
			{
				if(partlen > 0 && *attachcount < MAX_MAIL_PARTS)
				{
					mp->attach[*attachcount] = calloc(1, partlen+1);
					memcpy(mp->attach[*attachcount], part, partlen);
					(*attachcount)++;
				}
			}
		}
	}
}

/* Returns true if the line exists in the message buffer */
int findline(char *line, char *raw, unsigned long totlen)
{
	int z, linelen = strlen(line), lines = 0;
	char *start = raw;

	for(z=0;z<totlen;z++)
	{
		/* Found a new line */
		if(raw[z] == '\r')
		{
			int len = &raw[z] - start;

			lines++;

			if(len > linelen && strncasecmp(start, line, linelen) == 0)
				return lines;

			if(raw[z] == '\r' && raw[z+1] == '\n')
				z++;
			start = &raw[z+1];
		}
	}
	return 0;
}

char *headerends[] = { "Content-type:", "Subject:", "From:", "To:", NULL };

/* Find the body start point, and also fill in variables as necessary */
char *parse_message(char *raw, unsigned long totlen, MailParsed *mp)
{
	int z = 0, in_body = 0, in_header = 1, in_to = 0, tocount = 0, textcount = 0, htmlcount = 0, attachcount = 0;
	char *ret = raw, *start = raw, *part = NULL;
	char *boundary = NULL;
	int boundarylen = 0, type = TYPE_TEXT, rettype = TYPE_OTHR, headerindex = 0;

	if(mp)
	{
		memset(mp, 0, sizeof(MailParsed));

		/* Save the raw message */
		mp->raw = calloc(1, totlen+1);
		memcpy(mp->raw, raw, totlen);
	}

	/* Find the most optimal place to stop parsing the
	 * header... optimally this would be the Content-type
	 * line but some messages don't have it.
	 */
	while(headerends[z])
	{
		if(findline(headerends[z], raw, totlen))
		{
			headerindex = z;
			break;
		}
		z++;
	}

	for(z=0;z<totlen;z++)
	{
		/* Found a new line */
		if(raw[z] == '\r')
		{
			int len = &raw[z] - start;

			if(in_to && len > 1 && tocount < MAX_TO_ADDRESSES)
			{
				int itemlen = (len - 1) > MAIL_ITEM_MAX ? MAIL_ITEM_MAX : (len - 1);
				if(start[itemlen] == ';' || start[itemlen] == ',')
				{
					in_to = 1;
					itemlen--;
				}
				else
					in_to = 0;
				mp->to[tocount] = calloc(1, itemlen+1);
				strncpy(mp->to[tocount], &start[1], itemlen);
				tocount++;
			}
			if(boundary && (start[0] == '-' || start[0] == boundary[0]))
			{
				int x;

				for(x=0;x<=(len - boundarylen);x++)
				{
					if(strncmp(&start[x], boundary, boundarylen) == 0)
					{
						in_body = 0;
						check_part(mp, &start, part, raw, &ret, &rettype, type, &textcount, &htmlcount, &attachcount);
						part = NULL;
					}
				}
			}
			if(len > 14 && strncasecmp(start, "Content-type:", 13) == 0)
			{
				in_header = 0;

				/* This is a multipart message... find the boundary */
				if(strncasecmp(&start[14], "text/plain", 10) == 0)
				{
					type = TYPE_TEXT;
				}
				else if(strncasecmp(&start[14], "text/html", 9) == 0)
				{
					type = TYPE_HTML;
				}
				else
					type = TYPE_OTHR;
			}
			else if(in_header && headerindex)
			{
				char *header =  headerends[headerindex];
				int headerlen = strlen(header);

				if(len > headerlen && strncasecmp(start, header, headerlen) == 0)
					in_header = 0;
			}
			if(!in_body)
			{
				if(len)
				{
					int itemlen;

					if(len > 14 && strncasecmp(start, "Content-type:", 13) == 0)
					{
						/* This is a multipart message... find the boundary */
						if(strncasecmp(&start[14], "Multipart", 9) == 0)
						{
							/* Find the boundary */
							int n, worklen = 100 > (totlen - (z+22)) ? (totlen - (z+22)) : 100;

							for(n=0;n<worklen;n++)
							{
								if(strncasecmp(&start[14+n], "boundary", 8) == 0)
								{
									int place = 22+n;
									n = worklen;

									if(start[place] == '=')
									{
										int t;
										char *boundstart = &start[place+1];

										place++;
										if(start[place] == '"')
										{
											place++;
											boundstart++;
										}

										worklen = 100 > (totlen - place) ? (totlen - place) : 100;

										for(t=0;t<worklen;t++)
										{
											if(boundstart[t] == '"' || boundstart[t] == '\r' || boundstart[t] == '\n')
											{
												boundary = calloc(1, t+1);
												strncpy(boundary, boundstart, t);
												boundarylen = t;
												t=worklen;
											}
										}
									}
								}
							}
						}
					}
					if(mp)
					{
						/* Pull out useful items */
						if(len > 9 && strncasecmp(start, "Subject:", 8) == 0)
						{
							itemlen = (len - 9) > MAIL_ITEM_MAX ? MAIL_ITEM_MAX : (len - 9);
							mp->topic = calloc(1, itemlen+1);
							strncpy(mp->topic, &start[9], itemlen);
						}
						if(len > 10 && strncasecmp(start, "Reply-to:", 9) == 0)
						{
							itemlen = (len - 10) > MAIL_ITEM_MAX ? MAIL_ITEM_MAX : (len - 10);
							mp->replyto = calloc(1, itemlen+1);
							strncpy(mp->replyto, &start[10], itemlen);
						}
						if(len > 4 && strncasecmp(start, "To:", 3) == 0 && tocount < MAX_TO_ADDRESSES)
						{
							itemlen = (len - 4) > MAIL_ITEM_MAX ? MAIL_ITEM_MAX : (len - 4);
							if(start[3+itemlen] == ';' || start[3+itemlen] == ',')
							{
								in_to = 1;
								itemlen--;
							}
							parse_to(mp, &tocount, &start[4], itemlen);
						}
						if(len > 6 && strncasecmp(start, "From:", 5) == 0)
						{
							itemlen = (len - 6) > MAIL_ITEM_MAX ? MAIL_ITEM_MAX : (len - 6);
							mp->from = calloc(1, itemlen+1);
							strncpy(mp->from, &start[6], itemlen);
						}
						if(len > 6 && strncasecmp(start, "Date:", 5) == 0)
						{
							itemlen = (len - 6) > MAIL_ITEM_MAX ? MAIL_ITEM_MAX : (len - 6);
							mp->date = calloc(1, itemlen+1);
							strncpy(mp->date, &start[6], itemlen);
						}
					}
				}
				else if(!in_header)
				{
					if(raw[z] == '\r' && raw[z+1] == '\n')
						part = &raw[z+2];
					else
						part = &raw[z+1];

					in_body = 1;
				}
			}
			if(raw[z] == '\r' && raw[z+1] == '\n')
				z++;
			start = &raw[z+1];
		}
	}
	start = &raw[z];
	check_part(mp, &start, part, raw, &ret, &rettype, type, &textcount, &htmlcount, &attachcount);
	if(boundary)
		free(boundary);
	return ret;
}

/* Checks if a string is a valid IP address */
int isip(char *buf)
{
	int z = 0, dotcount = 0, value;
	char *start = buf;

	while(buf[z])
	{
		if(buf[z] == '.')
		{
			value = atoi(start);
			if(value > 255 || value < 0)
				return 0;
			start = &buf[z+1];
			dotcount++;
		}
		else if(!isdigit(buf[z]))
			return 0;
		z++;
	}

	value = atoi(start);
	if(value > 255 || value < 0)
		return 0;

	if(dotcount != 3 || z < 7 || z > 15)
		return 0;
	return 1;
}

/* Initialize CDATA and CTIME structures to the
 * current date and time.
 */
void init_date_time(CDATE *cdate, CTIME *ctime)
{
	time_t curtime = time(NULL);
	struct tm curtm, *ret;

#ifdef __UNIX__
	ret = localtime_r(&curtime, &curtm);
	if(ret)
	{
#else
	ret = localtime(&curtime);

	if(ret)
	{
		memcpy(&curtm, ret, sizeof(struct tm));
#endif
		cdate->day = curtm.tm_mday;
		cdate->month = curtm.tm_mon + 1;
		cdate->year = curtm.tm_year + 1900;
		ctime->hours = curtm.tm_hour;
		ctime->minutes = curtm.tm_min;
		ctime->seconds = curtm.tm_sec;
	}
}


