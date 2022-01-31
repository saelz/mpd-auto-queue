#ifndef MPD_AUTO_QUEUE_PARSER_H
#define MPD_AUTO_QUEUE_PARSER_H

#include "net.h"
struct xml_item;

struct xml_item{
	struct str name;
	struct str data;
	size_t child_count;
	struct xml_item **children;
	struct xml_item *parent;
};

struct xml_item*
parse_xml(struct str xml_data);

struct xml_item*
find_xml_item_child(struct xml_item *item,struct str name);

struct xml_item*
find_xml_item_root(struct xml_item *item);

void
free_xml_item(struct xml_item *item);

void
print_xml_item(struct xml_item *item,int depth);
#endif
