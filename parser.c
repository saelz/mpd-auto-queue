#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "log.h"

#define LENGTH(x) sizeof(x)/sizeof(*x)

enum PARSE_STATE{
	NONE,
	NAME,
	ATTR,
	DATA,
	SUB_ITEM
};

static int
find_next(struct str haystack,struct str needle,size_t *pos){
	size_t needle_pos = 0;
	for (; *pos <= haystack.size; (*pos)++) {
		if (needle.data[needle_pos] == haystack.data[*pos])
			needle_pos++;

		if (needle_pos >= needle.size)
			return 1;
	}
	return 0;
}

static struct xml_item*
new_xml_item(){
	struct xml_item *item;

	item = malloc(sizeof(*item));

	item->children = NULL;
	item->child_count = 0;

	item->parent = NULL;

	item->name.data = NULL;
	item->name.size = 0;

	item->data.data = NULL;
	item->data.size = 0;

	return item;

}

static void
append_xml_child(struct xml_item *item,struct xml_item *child){
	item->children = realloc(item->children,
							 (item->child_count+1)*sizeof(*item->children));

	if (item->children == NULL){
		log_data(LOG_ERROR,"Unable to allocate pointer");
		exit(1);
	}
	item->children[item->child_count] = child;
	item->child_count++;
	child->parent = item;
}

static int
is_end_tag(struct str xml_data,struct str name,size_t *pos){
	if ((*pos)+name.size+2 >= xml_data.size)
		return 0;

	(*pos)++;
	if (xml_data.data[*pos] != '/')
		return 0;

	(*pos)++;
	if (!strncmp(xml_data.data,name.data,name.size))
		return 0;

	(*pos) += name.size;

	if (xml_data.data[*pos] != '>')
		return 0;

	return 1;
}
static struct xml_item*
parse_xml_recur(struct str xml_data,size_t *pos){
	struct xml_item *item,*child;
	enum PARSE_STATE state = NONE;
	size_t check_pos;

	if (*pos >= xml_data.size)
		return NULL;

	item = new_xml_item();
	for (; *pos < xml_data.size; (*pos)++) {
		switch (state) {
		case NONE:
			if (xml_data.data[*pos] == '<'){
				item->name.data = xml_data.data+(*pos)+1;
				state = NAME;
			}
			break;
		case NAME:
			if (xml_data.data[*pos] == ' '){
				state = ATTR;
			} else if (xml_data.data[*pos] == '>'){
				item->data.data = xml_data.data+(*pos)+1;
				state = DATA;
			} else {
				item->name.size++;
			}
			break;
		case ATTR:
			if (xml_data.data[*pos] == '>'){
				item->data.data = xml_data.data+(*pos)+1;
				state = DATA;
			}
			break;
		case DATA:
			if (xml_data.data[*pos] == '<'){
				state = SUB_ITEM;
			} else {
				item->data.size++;
				break;
			}
			/* falls through */
		case SUB_ITEM:
			check_pos = *pos;
			if (is_end_tag(xml_data,item->name,&check_pos)){
				*pos = check_pos+1;
				return item;
			}

			child = parse_xml_recur(xml_data, pos);
			if (child != NULL)
				append_xml_child(item, child);
		}

	}


	return item;
}


struct xml_item*
parse_xml(struct str xml_data){
	struct str header     = {"<?xml",5};
	struct str header_end = {"?>",2};
	struct xml_item *item,*child;
	size_t pos = 0;


	if (!find_next(xml_data,header,&pos)){
		log_data(LOG_ERROR,"Unable to find XML declaration in provided data");
		return NULL;
	}

	if (!find_next(xml_data,header_end,&pos)){
		log_data(LOG_ERROR,"Incomplete XML declaration");
		return NULL;
	}

	item = new_xml_item();

	while ((child = parse_xml_recur(xml_data, &pos)) != NULL){
		append_xml_child(item, child);
	}

	return item;
}

struct xml_item *
find_xml_item_child(struct xml_item *item,struct str name){
	size_t i;
	struct xml_item *child;

	for(i = 0; i < item->child_count; i++){
		child = item->children[i];
		if (!strncmp(child->name.data,name.data,
					 (name.size > child->name.size ?
					  child->name.size : name.size)))
			return child;
	}
	return NULL;
}

struct xml_item *
find_xml_item_root(struct xml_item *item){
	if (item->parent != NULL)
		return find_xml_item_root(item->parent);
	return item;
}

void
free_xml_item(struct xml_item *item){
	size_t i;
	for(i = 0; i < item->child_count; i++)
		free_xml_item(item->children[i]);

	free(item->children);
	free(item);
}

void
print_xml_item(struct xml_item *item,int depth){
	size_t i;
	printf("%*c$%.*s^ -> $%.*s^\n",depth,' ',(int)item->name.size,item->name.data,
		   (int)item->data.size,item->data.data);

	for(i = 0; i < item->child_count; i++)
		print_xml_item(item->children[i],depth+1);
}
