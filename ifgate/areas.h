#ifndef AREA_LIST_TYPE
#define AREA_LIST_TYPE
typedef struct _area_list {
	struct _area_list *next;
	char *name;
} area_list;
#endif

extern int group_count;
extern area_list *areas(char *);
extern void ngdist(char*,char**,char**);
extern void tidy_arealist(area_list *);
