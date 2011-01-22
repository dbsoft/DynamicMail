#ifndef __USERENT_H__
#define __USERENT_H__

typedef struct _userentry {
	HWND window, entryfield;
	int page;
	char *filename;
	void *data, *data2;
	HWND *busy;
} UserEntry;

#endif
